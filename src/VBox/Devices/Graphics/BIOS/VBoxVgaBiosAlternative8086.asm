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





section VGAROM progbits vstart=0x0 align=1 ; size=0x94f class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x94f -> off=0x28 cb=0000000000000578 uValue=00000000000c0028 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0ebh, 01dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h, 00eh, 01fh, 0fch, 0e9h, 03eh, 00ah
vgabios_int10_handler:                       ; 0xc0028 LB 0x578
    pushfw                                    ; 9c                          ; 0xc0028 vgarom.asm:91
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0029 vgarom.asm:104
    jne short 00034h                          ; 75 06                       ; 0xc002c vgarom.asm:105
    call 0018dh                               ; e8 5c 01                    ; 0xc002e vgarom.asm:106
    jmp near 000fdh                           ; e9 c9 00                    ; 0xc0031 vgarom.asm:107
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc0034 vgarom.asm:109
    jne short 0003fh                          ; 75 06                       ; 0xc0037 vgarom.asm:110
    call 00560h                               ; e8 24 05                    ; 0xc0039 vgarom.asm:111
    jmp near 000fdh                           ; e9 be 00                    ; 0xc003c vgarom.asm:112
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc003f vgarom.asm:114
    jne short 0004ah                          ; 75 06                       ; 0xc0042 vgarom.asm:115
    call 000ffh                               ; e8 b8 00                    ; 0xc0044 vgarom.asm:116
    jmp near 000fdh                           ; e9 b3 00                    ; 0xc0047 vgarom.asm:117
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc004a vgarom.asm:119
    jne short 00055h                          ; 75 06                       ; 0xc004d vgarom.asm:120
    call 00454h                               ; e8 02 04                    ; 0xc004f vgarom.asm:121
    jmp near 000fdh                           ; e9 a8 00                    ; 0xc0052 vgarom.asm:122
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc0055 vgarom.asm:124
    jne short 00099h                          ; 75 3f                       ; 0xc0058 vgarom.asm:125
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc005a vgarom.asm:126
    jne short 00065h                          ; 75 06                       ; 0xc005d vgarom.asm:127
    call 00461h                               ; e8 ff 03                    ; 0xc005f vgarom.asm:128
    jmp near 000fdh                           ; e9 98 00                    ; 0xc0062 vgarom.asm:129
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc0065 vgarom.asm:131
    jne short 00070h                          ; 75 06                       ; 0xc0068 vgarom.asm:132
    call 00484h                               ; e8 17 04                    ; 0xc006a vgarom.asm:133
    jmp near 000fdh                           ; e9 8d 00                    ; 0xc006d vgarom.asm:134
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc0070 vgarom.asm:136
    jne short 0007bh                          ; 75 06                       ; 0xc0073 vgarom.asm:137
    call 004d7h                               ; e8 5f 04                    ; 0xc0075 vgarom.asm:138
    jmp near 000fdh                           ; e9 82 00                    ; 0xc0078 vgarom.asm:139
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc007b vgarom.asm:141
    jne short 00085h                          ; 75 05                       ; 0xc007e vgarom.asm:142
    call 004fch                               ; e8 79 04                    ; 0xc0080 vgarom.asm:143
    jmp short 000fdh                          ; eb 78                       ; 0xc0083 vgarom.asm:144
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc0085 vgarom.asm:146
    jne short 0008fh                          ; 75 05                       ; 0xc0088 vgarom.asm:147
    call 0051ah                               ; e8 8d 04                    ; 0xc008a vgarom.asm:148
    jmp short 000fdh                          ; eb 6e                       ; 0xc008d vgarom.asm:149
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc008f vgarom.asm:151
    jne short 000e3h                          ; 75 4f                       ; 0xc0092 vgarom.asm:152
    call 0053eh                               ; e8 a7 04                    ; 0xc0094 vgarom.asm:153
    jmp short 000fdh                          ; eb 64                       ; 0xc0097 vgarom.asm:154
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0099 vgarom.asm:156
    je short 000e3h                           ; 74 45                       ; 0xc009c vgarom.asm:157
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc009e vgarom.asm:158
    jne short 000a8h                          ; 75 05                       ; 0xc00a1 vgarom.asm:162
    call 001b4h                               ; e8 0e 01                    ; 0xc00a3 vgarom.asm:164
    jmp short 000fdh                          ; eb 55                       ; 0xc00a6 vgarom.asm:165
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a8 vgarom.asm:168
    jne short 000e3h                          ; 75 36                       ; 0xc00ab vgarom.asm:169
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00ad vgarom.asm:170
    jne short 000b6h                          ; 75 05                       ; 0xc00af vgarom.asm:171
    call 0080bh                               ; e8 57 07                    ; 0xc00b1 vgarom.asm:172
    jmp short 000fdh                          ; eb 47                       ; 0xc00b4 vgarom.asm:173
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00b6 vgarom.asm:175
    jne short 000bfh                          ; 75 05                       ; 0xc00b8 vgarom.asm:176
    call 00830h                               ; e8 73 07                    ; 0xc00ba vgarom.asm:177
    jmp short 000fdh                          ; eb 3e                       ; 0xc00bd vgarom.asm:178
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00bf vgarom.asm:180
    jne short 000c8h                          ; 75 05                       ; 0xc00c1 vgarom.asm:181
    call 0085dh                               ; e8 97 07                    ; 0xc00c3 vgarom.asm:182
    jmp short 000fdh                          ; eb 35                       ; 0xc00c6 vgarom.asm:183
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c8 vgarom.asm:185
    jne short 000d1h                          ; 75 05                       ; 0xc00ca vgarom.asm:186
    call 00891h                               ; e8 c2 07                    ; 0xc00cc vgarom.asm:187
    jmp short 000fdh                          ; eb 2c                       ; 0xc00cf vgarom.asm:188
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00d1 vgarom.asm:190
    jne short 000dah                          ; 75 05                       ; 0xc00d3 vgarom.asm:191
    call 008c8h                               ; e8 f0 07                    ; 0xc00d5 vgarom.asm:192
    jmp short 000fdh                          ; eb 23                       ; 0xc00d8 vgarom.asm:193
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00da vgarom.asm:195
    jne short 000e3h                          ; 75 05                       ; 0xc00dc vgarom.asm:196
    call 0093bh                               ; e8 5a 08                    ; 0xc00de vgarom.asm:197
    jmp short 000fdh                          ; eb 1a                       ; 0xc00e1 vgarom.asm:198
    push ES                                   ; 06                          ; 0xc00e3 vgarom.asm:202
    push DS                                   ; 1e                          ; 0xc00e4 vgarom.asm:203
    push ax                                   ; 50                          ; 0xc00e5 vgarom.asm:109
    push cx                                   ; 51                          ; 0xc00e6 vgarom.asm:110
    push dx                                   ; 52                          ; 0xc00e7 vgarom.asm:111
    push bx                                   ; 53                          ; 0xc00e8 vgarom.asm:112
    push sp                                   ; 54                          ; 0xc00e9 vgarom.asm:113
    push bp                                   ; 55                          ; 0xc00ea vgarom.asm:114
    push si                                   ; 56                          ; 0xc00eb vgarom.asm:115
    push di                                   ; 57                          ; 0xc00ec vgarom.asm:116
    push CS                                   ; 0e                          ; 0xc00ed vgarom.asm:207
    pop DS                                    ; 1f                          ; 0xc00ee vgarom.asm:208
    cld                                       ; fc                          ; 0xc00ef vgarom.asm:209
    call 03a03h                               ; e8 10 39                    ; 0xc00f0 vgarom.asm:210
    pop di                                    ; 5f                          ; 0xc00f3 vgarom.asm:126
    pop si                                    ; 5e                          ; 0xc00f4 vgarom.asm:127
    pop bp                                    ; 5d                          ; 0xc00f5 vgarom.asm:128
    pop bx                                    ; 5b                          ; 0xc00f6 vgarom.asm:129
    pop bx                                    ; 5b                          ; 0xc00f7 vgarom.asm:130
    pop dx                                    ; 5a                          ; 0xc00f8 vgarom.asm:131
    pop cx                                    ; 59                          ; 0xc00f9 vgarom.asm:132
    pop ax                                    ; 58                          ; 0xc00fa vgarom.asm:133
    pop DS                                    ; 1f                          ; 0xc00fb vgarom.asm:213
    pop ES                                    ; 07                          ; 0xc00fc vgarom.asm:214
    popfw                                     ; 9d                          ; 0xc00fd vgarom.asm:216
    iret                                      ; cf                          ; 0xc00fe vgarom.asm:217
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00ff vgarom.asm:222
    je short 0010ah                           ; 74 06                       ; 0xc0102 vgarom.asm:223
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc0104 vgarom.asm:224
    je short 0015bh                           ; 74 52                       ; 0xc0107 vgarom.asm:225
    retn                                      ; c3                          ; 0xc0109 vgarom.asm:229
    push ax                                   ; 50                          ; 0xc010a vgarom.asm:231
    push bx                                   ; 53                          ; 0xc010b vgarom.asm:232
    push cx                                   ; 51                          ; 0xc010c vgarom.asm:233
    push dx                                   ; 52                          ; 0xc010d vgarom.asm:234
    push DS                                   ; 1e                          ; 0xc010e vgarom.asm:235
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc010f vgarom.asm:236
    mov ds, dx                                ; 8e da                       ; 0xc0112 vgarom.asm:237
    mov dx, 003dah                            ; ba da 03                    ; 0xc0114 vgarom.asm:238
    in AL, DX                                 ; ec                          ; 0xc0117 vgarom.asm:239
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0118 vgarom.asm:240
    jbe short 0014eh                          ; 76 2f                       ; 0xc011d vgarom.asm:241
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc011f vgarom.asm:242
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc0122 vgarom.asm:243
    out DX, AL                                ; ee                          ; 0xc0124 vgarom.asm:244
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0125 vgarom.asm:245
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0127 vgarom.asm:246
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0129 vgarom.asm:247
    je short 0012fh                           ; 74 02                       ; 0xc012b vgarom.asm:248
    add AL, strict byte 008h                  ; 04 08                       ; 0xc012d vgarom.asm:249
    out DX, AL                                ; ee                          ; 0xc012f vgarom.asm:251
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0130 vgarom.asm:252
    and bl, 010h                              ; 80 e3 10                    ; 0xc0132 vgarom.asm:253
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0135 vgarom.asm:255
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0138 vgarom.asm:256
    out DX, AL                                ; ee                          ; 0xc013a vgarom.asm:257
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc013b vgarom.asm:258
    in AL, DX                                 ; ec                          ; 0xc013e vgarom.asm:259
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc013f vgarom.asm:260
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0141 vgarom.asm:261
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0143 vgarom.asm:262
    out DX, AL                                ; ee                          ; 0xc0146 vgarom.asm:263
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0147 vgarom.asm:264
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0149 vgarom.asm:265
    jne short 00135h                          ; 75 e7                       ; 0xc014c vgarom.asm:266
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc014e vgarom.asm:268
    out DX, AL                                ; ee                          ; 0xc0150 vgarom.asm:269
    mov dx, 003dah                            ; ba da 03                    ; 0xc0151 vgarom.asm:271
    in AL, DX                                 ; ec                          ; 0xc0154 vgarom.asm:272
    pop DS                                    ; 1f                          ; 0xc0155 vgarom.asm:274
    pop dx                                    ; 5a                          ; 0xc0156 vgarom.asm:275
    pop cx                                    ; 59                          ; 0xc0157 vgarom.asm:276
    pop bx                                    ; 5b                          ; 0xc0158 vgarom.asm:277
    pop ax                                    ; 58                          ; 0xc0159 vgarom.asm:278
    retn                                      ; c3                          ; 0xc015a vgarom.asm:279
    push ax                                   ; 50                          ; 0xc015b vgarom.asm:281
    push bx                                   ; 53                          ; 0xc015c vgarom.asm:282
    push cx                                   ; 51                          ; 0xc015d vgarom.asm:283
    push dx                                   ; 52                          ; 0xc015e vgarom.asm:284
    mov dx, 003dah                            ; ba da 03                    ; 0xc015f vgarom.asm:285
    in AL, DX                                 ; ec                          ; 0xc0162 vgarom.asm:286
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0163 vgarom.asm:287
    and bl, 001h                              ; 80 e3 01                    ; 0xc0165 vgarom.asm:288
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0168 vgarom.asm:290
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc016b vgarom.asm:291
    out DX, AL                                ; ee                          ; 0xc016d vgarom.asm:292
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc016e vgarom.asm:293
    in AL, DX                                 ; ec                          ; 0xc0171 vgarom.asm:294
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0172 vgarom.asm:295
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0174 vgarom.asm:296
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0176 vgarom.asm:297
    out DX, AL                                ; ee                          ; 0xc0179 vgarom.asm:298
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc017a vgarom.asm:299
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc017c vgarom.asm:300
    jne short 00168h                          ; 75 e7                       ; 0xc017f vgarom.asm:301
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0181 vgarom.asm:302
    out DX, AL                                ; ee                          ; 0xc0183 vgarom.asm:303
    mov dx, 003dah                            ; ba da 03                    ; 0xc0184 vgarom.asm:305
    in AL, DX                                 ; ec                          ; 0xc0187 vgarom.asm:306
    pop dx                                    ; 5a                          ; 0xc0188 vgarom.asm:308
    pop cx                                    ; 59                          ; 0xc0189 vgarom.asm:309
    pop bx                                    ; 5b                          ; 0xc018a vgarom.asm:310
    pop ax                                    ; 58                          ; 0xc018b vgarom.asm:311
    retn                                      ; c3                          ; 0xc018c vgarom.asm:312
    push DS                                   ; 1e                          ; 0xc018d vgarom.asm:317
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc018e vgarom.asm:318
    mov ds, ax                                ; 8e d8                       ; 0xc0191 vgarom.asm:319
    push bx                                   ; 53                          ; 0xc0193 vgarom.asm:320
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc0194 vgarom.asm:321
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0197 vgarom.asm:322
    pop bx                                    ; 5b                          ; 0xc0199 vgarom.asm:323
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc019a vgarom.asm:324
    push bx                                   ; 53                          ; 0xc019c vgarom.asm:325
    mov bx, 00087h                            ; bb 87 00                    ; 0xc019d vgarom.asm:326
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc01a0 vgarom.asm:327
    and ah, 080h                              ; 80 e4 80                    ; 0xc01a2 vgarom.asm:328
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc01a5 vgarom.asm:329
    mov al, byte [bx]                         ; 8a 07                       ; 0xc01a8 vgarom.asm:330
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc01aa vgarom.asm:331
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc01ac vgarom.asm:332
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc01af vgarom.asm:333
    pop bx                                    ; 5b                          ; 0xc01b1 vgarom.asm:334
    pop DS                                    ; 1f                          ; 0xc01b2 vgarom.asm:335
    retn                                      ; c3                          ; 0xc01b3 vgarom.asm:336
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01b4 vgarom.asm:341
    jne short 001bah                          ; 75 02                       ; 0xc01b6 vgarom.asm:342
    jmp short 0021bh                          ; eb 61                       ; 0xc01b8 vgarom.asm:343
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01ba vgarom.asm:345
    jne short 001c0h                          ; 75 02                       ; 0xc01bc vgarom.asm:346
    jmp short 00239h                          ; eb 79                       ; 0xc01be vgarom.asm:347
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01c0 vgarom.asm:349
    jne short 001c6h                          ; 75 02                       ; 0xc01c2 vgarom.asm:350
    jmp short 00241h                          ; eb 7b                       ; 0xc01c4 vgarom.asm:351
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01c6 vgarom.asm:353
    jne short 001cdh                          ; 75 03                       ; 0xc01c8 vgarom.asm:354
    jmp near 00272h                           ; e9 a5 00                    ; 0xc01ca vgarom.asm:355
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01cd vgarom.asm:357
    jne short 001d4h                          ; 75 03                       ; 0xc01cf vgarom.asm:358
    jmp near 0029fh                           ; e9 cb 00                    ; 0xc01d1 vgarom.asm:359
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01d4 vgarom.asm:361
    jne short 001dbh                          ; 75 03                       ; 0xc01d6 vgarom.asm:362
    jmp near 002c7h                           ; e9 ec 00                    ; 0xc01d8 vgarom.asm:363
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01db vgarom.asm:365
    jne short 001e2h                          ; 75 03                       ; 0xc01dd vgarom.asm:366
    jmp near 002d5h                           ; e9 f3 00                    ; 0xc01df vgarom.asm:367
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01e2 vgarom.asm:369
    jne short 001e9h                          ; 75 03                       ; 0xc01e4 vgarom.asm:370
    jmp near 0031ah                           ; e9 31 01                    ; 0xc01e6 vgarom.asm:371
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01e9 vgarom.asm:373
    jne short 001f0h                          ; 75 03                       ; 0xc01eb vgarom.asm:374
    jmp near 00333h                           ; e9 43 01                    ; 0xc01ed vgarom.asm:375
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01f0 vgarom.asm:377
    jne short 001f7h                          ; 75 03                       ; 0xc01f2 vgarom.asm:378
    jmp near 0035bh                           ; e9 64 01                    ; 0xc01f4 vgarom.asm:379
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01f7 vgarom.asm:381
    jne short 001feh                          ; 75 03                       ; 0xc01f9 vgarom.asm:382
    jmp near 003aeh                           ; e9 b0 01                    ; 0xc01fb vgarom.asm:383
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01fe vgarom.asm:385
    jne short 00205h                          ; 75 03                       ; 0xc0200 vgarom.asm:386
    jmp near 003c9h                           ; e9 c4 01                    ; 0xc0202 vgarom.asm:387
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc0205 vgarom.asm:389
    jne short 0020ch                          ; 75 03                       ; 0xc0207 vgarom.asm:390
    jmp near 003f1h                           ; e9 e5 01                    ; 0xc0209 vgarom.asm:391
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc020c vgarom.asm:393
    jne short 00213h                          ; 75 03                       ; 0xc020e vgarom.asm:394
    jmp near 003fch                           ; e9 e9 01                    ; 0xc0210 vgarom.asm:395
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc0213 vgarom.asm:397
    jne short 0021ah                          ; 75 03                       ; 0xc0215 vgarom.asm:398
    jmp near 00407h                           ; e9 ed 01                    ; 0xc0217 vgarom.asm:399
    retn                                      ; c3                          ; 0xc021a vgarom.asm:404
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc021b vgarom.asm:407
    jnbe short 00238h                         ; 77 18                       ; 0xc021e vgarom.asm:408
    push ax                                   ; 50                          ; 0xc0220 vgarom.asm:409
    push dx                                   ; 52                          ; 0xc0221 vgarom.asm:410
    mov dx, 003dah                            ; ba da 03                    ; 0xc0222 vgarom.asm:411
    in AL, DX                                 ; ec                          ; 0xc0225 vgarom.asm:412
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0226 vgarom.asm:413
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0229 vgarom.asm:414
    out DX, AL                                ; ee                          ; 0xc022b vgarom.asm:415
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc022c vgarom.asm:416
    out DX, AL                                ; ee                          ; 0xc022e vgarom.asm:417
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc022f vgarom.asm:418
    out DX, AL                                ; ee                          ; 0xc0231 vgarom.asm:419
    mov dx, 003dah                            ; ba da 03                    ; 0xc0232 vgarom.asm:421
    in AL, DX                                 ; ec                          ; 0xc0235 vgarom.asm:422
    pop dx                                    ; 5a                          ; 0xc0236 vgarom.asm:424
    pop ax                                    ; 58                          ; 0xc0237 vgarom.asm:425
    retn                                      ; c3                          ; 0xc0238 vgarom.asm:427
    push bx                                   ; 53                          ; 0xc0239 vgarom.asm:432
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc023a vgarom.asm:433
    call 0021bh                               ; e8 dc ff                    ; 0xc023c vgarom.asm:434
    pop bx                                    ; 5b                          ; 0xc023f vgarom.asm:435
    retn                                      ; c3                          ; 0xc0240 vgarom.asm:436
    push ax                                   ; 50                          ; 0xc0241 vgarom.asm:441
    push bx                                   ; 53                          ; 0xc0242 vgarom.asm:442
    push cx                                   ; 51                          ; 0xc0243 vgarom.asm:443
    push dx                                   ; 52                          ; 0xc0244 vgarom.asm:444
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0245 vgarom.asm:445
    mov dx, 003dah                            ; ba da 03                    ; 0xc0247 vgarom.asm:446
    in AL, DX                                 ; ec                          ; 0xc024a vgarom.asm:447
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc024b vgarom.asm:448
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc024d vgarom.asm:449
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0250 vgarom.asm:451
    out DX, AL                                ; ee                          ; 0xc0252 vgarom.asm:452
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0253 vgarom.asm:453
    out DX, AL                                ; ee                          ; 0xc0256 vgarom.asm:454
    inc bx                                    ; 43                          ; 0xc0257 vgarom.asm:455
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0258 vgarom.asm:456
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc025a vgarom.asm:457
    jne short 00250h                          ; 75 f1                       ; 0xc025d vgarom.asm:458
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc025f vgarom.asm:459
    out DX, AL                                ; ee                          ; 0xc0261 vgarom.asm:460
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0262 vgarom.asm:461
    out DX, AL                                ; ee                          ; 0xc0265 vgarom.asm:462
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0266 vgarom.asm:463
    out DX, AL                                ; ee                          ; 0xc0268 vgarom.asm:464
    mov dx, 003dah                            ; ba da 03                    ; 0xc0269 vgarom.asm:466
    in AL, DX                                 ; ec                          ; 0xc026c vgarom.asm:467
    pop dx                                    ; 5a                          ; 0xc026d vgarom.asm:469
    pop cx                                    ; 59                          ; 0xc026e vgarom.asm:470
    pop bx                                    ; 5b                          ; 0xc026f vgarom.asm:471
    pop ax                                    ; 58                          ; 0xc0270 vgarom.asm:472
    retn                                      ; c3                          ; 0xc0271 vgarom.asm:473
    push ax                                   ; 50                          ; 0xc0272 vgarom.asm:478
    push bx                                   ; 53                          ; 0xc0273 vgarom.asm:479
    push dx                                   ; 52                          ; 0xc0274 vgarom.asm:480
    mov dx, 003dah                            ; ba da 03                    ; 0xc0275 vgarom.asm:481
    in AL, DX                                 ; ec                          ; 0xc0278 vgarom.asm:482
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0279 vgarom.asm:483
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc027c vgarom.asm:484
    out DX, AL                                ; ee                          ; 0xc027e vgarom.asm:485
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc027f vgarom.asm:486
    in AL, DX                                 ; ec                          ; 0xc0282 vgarom.asm:487
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc0283 vgarom.asm:488
    and bl, 001h                              ; 80 e3 01                    ; 0xc0285 vgarom.asm:489
    sal bl, 1                                 ; d0 e3                       ; 0xc0288 vgarom.asm:493
    sal bl, 1                                 ; d0 e3                       ; 0xc028a vgarom.asm:494
    sal bl, 1                                 ; d0 e3                       ; 0xc028c vgarom.asm:495
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc028e vgarom.asm:497
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0290 vgarom.asm:498
    out DX, AL                                ; ee                          ; 0xc0293 vgarom.asm:499
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0294 vgarom.asm:500
    out DX, AL                                ; ee                          ; 0xc0296 vgarom.asm:501
    mov dx, 003dah                            ; ba da 03                    ; 0xc0297 vgarom.asm:503
    in AL, DX                                 ; ec                          ; 0xc029a vgarom.asm:504
    pop dx                                    ; 5a                          ; 0xc029b vgarom.asm:506
    pop bx                                    ; 5b                          ; 0xc029c vgarom.asm:507
    pop ax                                    ; 58                          ; 0xc029d vgarom.asm:508
    retn                                      ; c3                          ; 0xc029e vgarom.asm:509
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc029f vgarom.asm:514
    jnbe short 002c6h                         ; 77 22                       ; 0xc02a2 vgarom.asm:515
    push ax                                   ; 50                          ; 0xc02a4 vgarom.asm:516
    push dx                                   ; 52                          ; 0xc02a5 vgarom.asm:517
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a6 vgarom.asm:518
    in AL, DX                                 ; ec                          ; 0xc02a9 vgarom.asm:519
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02aa vgarom.asm:520
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc02ad vgarom.asm:521
    out DX, AL                                ; ee                          ; 0xc02af vgarom.asm:522
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02b0 vgarom.asm:523
    in AL, DX                                 ; ec                          ; 0xc02b3 vgarom.asm:524
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02b4 vgarom.asm:525
    mov dx, 003dah                            ; ba da 03                    ; 0xc02b6 vgarom.asm:526
    in AL, DX                                 ; ec                          ; 0xc02b9 vgarom.asm:527
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02ba vgarom.asm:528
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02bd vgarom.asm:529
    out DX, AL                                ; ee                          ; 0xc02bf vgarom.asm:530
    mov dx, 003dah                            ; ba da 03                    ; 0xc02c0 vgarom.asm:532
    in AL, DX                                 ; ec                          ; 0xc02c3 vgarom.asm:533
    pop dx                                    ; 5a                          ; 0xc02c4 vgarom.asm:535
    pop ax                                    ; 58                          ; 0xc02c5 vgarom.asm:536
    retn                                      ; c3                          ; 0xc02c6 vgarom.asm:538
    push ax                                   ; 50                          ; 0xc02c7 vgarom.asm:543
    push bx                                   ; 53                          ; 0xc02c8 vgarom.asm:544
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02c9 vgarom.asm:545
    call 0029fh                               ; e8 d1 ff                    ; 0xc02cb vgarom.asm:546
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02ce vgarom.asm:547
    pop bx                                    ; 5b                          ; 0xc02d0 vgarom.asm:548
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02d1 vgarom.asm:549
    pop ax                                    ; 58                          ; 0xc02d3 vgarom.asm:550
    retn                                      ; c3                          ; 0xc02d4 vgarom.asm:551
    push ax                                   ; 50                          ; 0xc02d5 vgarom.asm:556
    push bx                                   ; 53                          ; 0xc02d6 vgarom.asm:557
    push cx                                   ; 51                          ; 0xc02d7 vgarom.asm:558
    push dx                                   ; 52                          ; 0xc02d8 vgarom.asm:559
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02d9 vgarom.asm:560
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02db vgarom.asm:561
    mov dx, 003dah                            ; ba da 03                    ; 0xc02dd vgarom.asm:563
    in AL, DX                                 ; ec                          ; 0xc02e0 vgarom.asm:564
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02e1 vgarom.asm:565
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02e4 vgarom.asm:566
    out DX, AL                                ; ee                          ; 0xc02e6 vgarom.asm:567
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02e7 vgarom.asm:568
    in AL, DX                                 ; ec                          ; 0xc02ea vgarom.asm:569
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02eb vgarom.asm:570
    inc bx                                    ; 43                          ; 0xc02ee vgarom.asm:571
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02ef vgarom.asm:572
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02f1 vgarom.asm:573
    jne short 002ddh                          ; 75 e7                       ; 0xc02f4 vgarom.asm:574
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f6 vgarom.asm:575
    in AL, DX                                 ; ec                          ; 0xc02f9 vgarom.asm:576
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02fa vgarom.asm:577
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02fd vgarom.asm:578
    out DX, AL                                ; ee                          ; 0xc02ff vgarom.asm:579
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0300 vgarom.asm:580
    in AL, DX                                 ; ec                          ; 0xc0303 vgarom.asm:581
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc0304 vgarom.asm:582
    mov dx, 003dah                            ; ba da 03                    ; 0xc0307 vgarom.asm:583
    in AL, DX                                 ; ec                          ; 0xc030a vgarom.asm:584
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc030b vgarom.asm:585
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc030e vgarom.asm:586
    out DX, AL                                ; ee                          ; 0xc0310 vgarom.asm:587
    mov dx, 003dah                            ; ba da 03                    ; 0xc0311 vgarom.asm:589
    in AL, DX                                 ; ec                          ; 0xc0314 vgarom.asm:590
    pop dx                                    ; 5a                          ; 0xc0315 vgarom.asm:592
    pop cx                                    ; 59                          ; 0xc0316 vgarom.asm:593
    pop bx                                    ; 5b                          ; 0xc0317 vgarom.asm:594
    pop ax                                    ; 58                          ; 0xc0318 vgarom.asm:595
    retn                                      ; c3                          ; 0xc0319 vgarom.asm:596
    push ax                                   ; 50                          ; 0xc031a vgarom.asm:601
    push dx                                   ; 52                          ; 0xc031b vgarom.asm:602
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc031c vgarom.asm:603
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc031f vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc0321 vgarom.asm:605
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0322 vgarom.asm:606
    pop ax                                    ; 58                          ; 0xc0325 vgarom.asm:607
    push ax                                   ; 50                          ; 0xc0326 vgarom.asm:608
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0327 vgarom.asm:609
    out DX, AL                                ; ee                          ; 0xc0329 vgarom.asm:610
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc032a vgarom.asm:611
    out DX, AL                                ; ee                          ; 0xc032c vgarom.asm:612
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc032d vgarom.asm:613
    out DX, AL                                ; ee                          ; 0xc032f vgarom.asm:614
    pop dx                                    ; 5a                          ; 0xc0330 vgarom.asm:615
    pop ax                                    ; 58                          ; 0xc0331 vgarom.asm:616
    retn                                      ; c3                          ; 0xc0332 vgarom.asm:617
    push ax                                   ; 50                          ; 0xc0333 vgarom.asm:622
    push bx                                   ; 53                          ; 0xc0334 vgarom.asm:623
    push cx                                   ; 51                          ; 0xc0335 vgarom.asm:624
    push dx                                   ; 52                          ; 0xc0336 vgarom.asm:625
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0337 vgarom.asm:626
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc033a vgarom.asm:627
    out DX, AL                                ; ee                          ; 0xc033c vgarom.asm:628
    pop dx                                    ; 5a                          ; 0xc033d vgarom.asm:629
    push dx                                   ; 52                          ; 0xc033e vgarom.asm:630
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc033f vgarom.asm:631
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0341 vgarom.asm:632
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0344 vgarom.asm:634
    out DX, AL                                ; ee                          ; 0xc0347 vgarom.asm:635
    inc bx                                    ; 43                          ; 0xc0348 vgarom.asm:636
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0349 vgarom.asm:637
    out DX, AL                                ; ee                          ; 0xc034c vgarom.asm:638
    inc bx                                    ; 43                          ; 0xc034d vgarom.asm:639
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc034e vgarom.asm:640
    out DX, AL                                ; ee                          ; 0xc0351 vgarom.asm:641
    inc bx                                    ; 43                          ; 0xc0352 vgarom.asm:642
    dec cx                                    ; 49                          ; 0xc0353 vgarom.asm:643
    jne short 00344h                          ; 75 ee                       ; 0xc0354 vgarom.asm:644
    pop dx                                    ; 5a                          ; 0xc0356 vgarom.asm:645
    pop cx                                    ; 59                          ; 0xc0357 vgarom.asm:646
    pop bx                                    ; 5b                          ; 0xc0358 vgarom.asm:647
    pop ax                                    ; 58                          ; 0xc0359 vgarom.asm:648
    retn                                      ; c3                          ; 0xc035a vgarom.asm:649
    push ax                                   ; 50                          ; 0xc035b vgarom.asm:654
    push bx                                   ; 53                          ; 0xc035c vgarom.asm:655
    push dx                                   ; 52                          ; 0xc035d vgarom.asm:656
    mov dx, 003dah                            ; ba da 03                    ; 0xc035e vgarom.asm:657
    in AL, DX                                 ; ec                          ; 0xc0361 vgarom.asm:658
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0362 vgarom.asm:659
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0365 vgarom.asm:660
    out DX, AL                                ; ee                          ; 0xc0367 vgarom.asm:661
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0368 vgarom.asm:662
    in AL, DX                                 ; ec                          ; 0xc036b vgarom.asm:663
    and bl, 001h                              ; 80 e3 01                    ; 0xc036c vgarom.asm:664
    jne short 00389h                          ; 75 18                       ; 0xc036f vgarom.asm:665
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc0371 vgarom.asm:666
    sal bh, 1                                 ; d0 e7                       ; 0xc0373 vgarom.asm:670
    sal bh, 1                                 ; d0 e7                       ; 0xc0375 vgarom.asm:671
    sal bh, 1                                 ; d0 e7                       ; 0xc0377 vgarom.asm:672
    sal bh, 1                                 ; d0 e7                       ; 0xc0379 vgarom.asm:673
    sal bh, 1                                 ; d0 e7                       ; 0xc037b vgarom.asm:674
    sal bh, 1                                 ; d0 e7                       ; 0xc037d vgarom.asm:675
    sal bh, 1                                 ; d0 e7                       ; 0xc037f vgarom.asm:676
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc0381 vgarom.asm:678
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0383 vgarom.asm:679
    out DX, AL                                ; ee                          ; 0xc0386 vgarom.asm:680
    jmp short 003a3h                          ; eb 1a                       ; 0xc0387 vgarom.asm:681
    push ax                                   ; 50                          ; 0xc0389 vgarom.asm:683
    mov dx, 003dah                            ; ba da 03                    ; 0xc038a vgarom.asm:684
    in AL, DX                                 ; ec                          ; 0xc038d vgarom.asm:685
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc038e vgarom.asm:686
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0391 vgarom.asm:687
    out DX, AL                                ; ee                          ; 0xc0393 vgarom.asm:688
    pop ax                                    ; 58                          ; 0xc0394 vgarom.asm:689
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0395 vgarom.asm:690
    jne short 0039dh                          ; 75 04                       ; 0xc0397 vgarom.asm:691
    sal bh, 1                                 ; d0 e7                       ; 0xc0399 vgarom.asm:695
    sal bh, 1                                 ; d0 e7                       ; 0xc039b vgarom.asm:696
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc039d vgarom.asm:699
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc03a0 vgarom.asm:700
    out DX, AL                                ; ee                          ; 0xc03a2 vgarom.asm:701
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc03a3 vgarom.asm:703
    out DX, AL                                ; ee                          ; 0xc03a5 vgarom.asm:704
    mov dx, 003dah                            ; ba da 03                    ; 0xc03a6 vgarom.asm:706
    in AL, DX                                 ; ec                          ; 0xc03a9 vgarom.asm:707
    pop dx                                    ; 5a                          ; 0xc03aa vgarom.asm:709
    pop bx                                    ; 5b                          ; 0xc03ab vgarom.asm:710
    pop ax                                    ; 58                          ; 0xc03ac vgarom.asm:711
    retn                                      ; c3                          ; 0xc03ad vgarom.asm:712
    push ax                                   ; 50                          ; 0xc03ae vgarom.asm:717
    push dx                                   ; 52                          ; 0xc03af vgarom.asm:718
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03b0 vgarom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03b3 vgarom.asm:720
    out DX, AL                                ; ee                          ; 0xc03b5 vgarom.asm:721
    pop ax                                    ; 58                          ; 0xc03b6 vgarom.asm:722
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc03b7 vgarom.asm:723
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b9 vgarom.asm:724
    in AL, DX                                 ; ec                          ; 0xc03bc vgarom.asm:725
    xchg al, ah                               ; 86 e0                       ; 0xc03bd vgarom.asm:726
    push ax                                   ; 50                          ; 0xc03bf vgarom.asm:727
    in AL, DX                                 ; ec                          ; 0xc03c0 vgarom.asm:728
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc03c1 vgarom.asm:729
    in AL, DX                                 ; ec                          ; 0xc03c3 vgarom.asm:730
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03c4 vgarom.asm:731
    pop dx                                    ; 5a                          ; 0xc03c6 vgarom.asm:732
    pop ax                                    ; 58                          ; 0xc03c7 vgarom.asm:733
    retn                                      ; c3                          ; 0xc03c8 vgarom.asm:734
    push ax                                   ; 50                          ; 0xc03c9 vgarom.asm:739
    push bx                                   ; 53                          ; 0xc03ca vgarom.asm:740
    push cx                                   ; 51                          ; 0xc03cb vgarom.asm:741
    push dx                                   ; 52                          ; 0xc03cc vgarom.asm:742
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03cd vgarom.asm:743
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03d0 vgarom.asm:744
    out DX, AL                                ; ee                          ; 0xc03d2 vgarom.asm:745
    pop dx                                    ; 5a                          ; 0xc03d3 vgarom.asm:746
    push dx                                   ; 52                          ; 0xc03d4 vgarom.asm:747
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03d5 vgarom.asm:748
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03d7 vgarom.asm:749
    in AL, DX                                 ; ec                          ; 0xc03da vgarom.asm:751
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03db vgarom.asm:752
    inc bx                                    ; 43                          ; 0xc03de vgarom.asm:753
    in AL, DX                                 ; ec                          ; 0xc03df vgarom.asm:754
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03e0 vgarom.asm:755
    inc bx                                    ; 43                          ; 0xc03e3 vgarom.asm:756
    in AL, DX                                 ; ec                          ; 0xc03e4 vgarom.asm:757
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03e5 vgarom.asm:758
    inc bx                                    ; 43                          ; 0xc03e8 vgarom.asm:759
    dec cx                                    ; 49                          ; 0xc03e9 vgarom.asm:760
    jne short 003dah                          ; 75 ee                       ; 0xc03ea vgarom.asm:761
    pop dx                                    ; 5a                          ; 0xc03ec vgarom.asm:762
    pop cx                                    ; 59                          ; 0xc03ed vgarom.asm:763
    pop bx                                    ; 5b                          ; 0xc03ee vgarom.asm:764
    pop ax                                    ; 58                          ; 0xc03ef vgarom.asm:765
    retn                                      ; c3                          ; 0xc03f0 vgarom.asm:766
    push ax                                   ; 50                          ; 0xc03f1 vgarom.asm:771
    push dx                                   ; 52                          ; 0xc03f2 vgarom.asm:772
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03f3 vgarom.asm:773
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03f6 vgarom.asm:774
    out DX, AL                                ; ee                          ; 0xc03f8 vgarom.asm:775
    pop dx                                    ; 5a                          ; 0xc03f9 vgarom.asm:776
    pop ax                                    ; 58                          ; 0xc03fa vgarom.asm:777
    retn                                      ; c3                          ; 0xc03fb vgarom.asm:778
    push ax                                   ; 50                          ; 0xc03fc vgarom.asm:783
    push dx                                   ; 52                          ; 0xc03fd vgarom.asm:784
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03fe vgarom.asm:785
    in AL, DX                                 ; ec                          ; 0xc0401 vgarom.asm:786
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0402 vgarom.asm:787
    pop dx                                    ; 5a                          ; 0xc0404 vgarom.asm:788
    pop ax                                    ; 58                          ; 0xc0405 vgarom.asm:789
    retn                                      ; c3                          ; 0xc0406 vgarom.asm:790
    push ax                                   ; 50                          ; 0xc0407 vgarom.asm:795
    push dx                                   ; 52                          ; 0xc0408 vgarom.asm:796
    mov dx, 003dah                            ; ba da 03                    ; 0xc0409 vgarom.asm:797
    in AL, DX                                 ; ec                          ; 0xc040c vgarom.asm:798
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc040d vgarom.asm:799
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0410 vgarom.asm:800
    out DX, AL                                ; ee                          ; 0xc0412 vgarom.asm:801
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0413 vgarom.asm:802
    in AL, DX                                 ; ec                          ; 0xc0416 vgarom.asm:803
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0417 vgarom.asm:804
    shr bl, 1                                 ; d0 eb                       ; 0xc0419 vgarom.asm:808
    shr bl, 1                                 ; d0 eb                       ; 0xc041b vgarom.asm:809
    shr bl, 1                                 ; d0 eb                       ; 0xc041d vgarom.asm:810
    shr bl, 1                                 ; d0 eb                       ; 0xc041f vgarom.asm:811
    shr bl, 1                                 ; d0 eb                       ; 0xc0421 vgarom.asm:812
    shr bl, 1                                 ; d0 eb                       ; 0xc0423 vgarom.asm:813
    shr bl, 1                                 ; d0 eb                       ; 0xc0425 vgarom.asm:814
    mov dx, 003dah                            ; ba da 03                    ; 0xc0427 vgarom.asm:816
    in AL, DX                                 ; ec                          ; 0xc042a vgarom.asm:817
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc042b vgarom.asm:818
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc042e vgarom.asm:819
    out DX, AL                                ; ee                          ; 0xc0430 vgarom.asm:820
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0431 vgarom.asm:821
    in AL, DX                                 ; ec                          ; 0xc0434 vgarom.asm:822
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0435 vgarom.asm:823
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0437 vgarom.asm:824
    test bl, 001h                             ; f6 c3 01                    ; 0xc043a vgarom.asm:825
    jne short 00443h                          ; 75 04                       ; 0xc043d vgarom.asm:826
    shr bh, 1                                 ; d0 ef                       ; 0xc043f vgarom.asm:830
    shr bh, 1                                 ; d0 ef                       ; 0xc0441 vgarom.asm:831
    mov dx, 003dah                            ; ba da 03                    ; 0xc0443 vgarom.asm:834
    in AL, DX                                 ; ec                          ; 0xc0446 vgarom.asm:835
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0447 vgarom.asm:836
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc044a vgarom.asm:837
    out DX, AL                                ; ee                          ; 0xc044c vgarom.asm:838
    mov dx, 003dah                            ; ba da 03                    ; 0xc044d vgarom.asm:840
    in AL, DX                                 ; ec                          ; 0xc0450 vgarom.asm:841
    pop dx                                    ; 5a                          ; 0xc0451 vgarom.asm:843
    pop ax                                    ; 58                          ; 0xc0452 vgarom.asm:844
    retn                                      ; c3                          ; 0xc0453 vgarom.asm:845
    push ax                                   ; 50                          ; 0xc0454 vgarom.asm:850
    push dx                                   ; 52                          ; 0xc0455 vgarom.asm:851
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0456 vgarom.asm:852
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc0459 vgarom.asm:853
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc045b vgarom.asm:854
    out DX, ax                                ; ef                          ; 0xc045d vgarom.asm:855
    pop dx                                    ; 5a                          ; 0xc045e vgarom.asm:856
    pop ax                                    ; 58                          ; 0xc045f vgarom.asm:857
    retn                                      ; c3                          ; 0xc0460 vgarom.asm:858
    push DS                                   ; 1e                          ; 0xc0461 vgarom.asm:863
    push ax                                   ; 50                          ; 0xc0462 vgarom.asm:864
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0463 vgarom.asm:865
    mov ds, ax                                ; 8e d8                       ; 0xc0466 vgarom.asm:866
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc0468 vgarom.asm:867
    mov bx, 00088h                            ; bb 88 00                    ; 0xc046a vgarom.asm:868
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc046d vgarom.asm:869
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc046f vgarom.asm:870
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc0472 vgarom.asm:871
    mov ax, word [bx]                         ; 8b 07                       ; 0xc0475 vgarom.asm:872
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0477 vgarom.asm:873
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc047a vgarom.asm:874
    jne short 00481h                          ; 75 02                       ; 0xc047d vgarom.asm:875
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc047f vgarom.asm:876
    pop ax                                    ; 58                          ; 0xc0481 vgarom.asm:878
    pop DS                                    ; 1f                          ; 0xc0482 vgarom.asm:879
    retn                                      ; c3                          ; 0xc0483 vgarom.asm:880
    push DS                                   ; 1e                          ; 0xc0484 vgarom.asm:888
    push bx                                   ; 53                          ; 0xc0485 vgarom.asm:889
    push dx                                   ; 52                          ; 0xc0486 vgarom.asm:890
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0487 vgarom.asm:891
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0489 vgarom.asm:892
    mov ds, ax                                ; 8e d8                       ; 0xc048c vgarom.asm:893
    mov bx, 00089h                            ; bb 89 00                    ; 0xc048e vgarom.asm:894
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0491 vgarom.asm:895
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0493 vgarom.asm:896
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0496 vgarom.asm:897
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc0498 vgarom.asm:898
    je short 004b2h                           ; 74 15                       ; 0xc049b vgarom.asm:899
    jc short 004bch                           ; 72 1d                       ; 0xc049d vgarom.asm:900
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc049f vgarom.asm:901
    je short 004a6h                           ; 74 02                       ; 0xc04a2 vgarom.asm:902
    jmp short 004d0h                          ; eb 2a                       ; 0xc04a4 vgarom.asm:912
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc04a6 vgarom.asm:918
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc04a8 vgarom.asm:919
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04aa vgarom.asm:920
    or ah, 009h                               ; 80 cc 09                    ; 0xc04ad vgarom.asm:921
    jne short 004c6h                          ; 75 14                       ; 0xc04b0 vgarom.asm:922
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc04b2 vgarom.asm:928
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04b4 vgarom.asm:929
    or ah, 009h                               ; 80 cc 09                    ; 0xc04b7 vgarom.asm:930
    jne short 004c6h                          ; 75 0a                       ; 0xc04ba vgarom.asm:931
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc04bc vgarom.asm:937
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc04be vgarom.asm:938
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04c0 vgarom.asm:939
    or ah, 008h                               ; 80 cc 08                    ; 0xc04c3 vgarom.asm:940
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04c6 vgarom.asm:942
    mov byte [bx], al                         ; 88 07                       ; 0xc04c9 vgarom.asm:943
    mov bx, 00088h                            ; bb 88 00                    ; 0xc04cb vgarom.asm:944
    mov byte [bx], ah                         ; 88 27                       ; 0xc04ce vgarom.asm:945
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04d0 vgarom.asm:947
    pop dx                                    ; 5a                          ; 0xc04d3 vgarom.asm:948
    pop bx                                    ; 5b                          ; 0xc04d4 vgarom.asm:949
    pop DS                                    ; 1f                          ; 0xc04d5 vgarom.asm:950
    retn                                      ; c3                          ; 0xc04d6 vgarom.asm:951
    push DS                                   ; 1e                          ; 0xc04d7 vgarom.asm:960
    push bx                                   ; 53                          ; 0xc04d8 vgarom.asm:961
    push dx                                   ; 52                          ; 0xc04d9 vgarom.asm:962
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04da vgarom.asm:963
    and dl, 001h                              ; 80 e2 01                    ; 0xc04dc vgarom.asm:964
    sal dl, 1                                 ; d0 e2                       ; 0xc04df vgarom.asm:968
    sal dl, 1                                 ; d0 e2                       ; 0xc04e1 vgarom.asm:969
    sal dl, 1                                 ; d0 e2                       ; 0xc04e3 vgarom.asm:970
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04e5 vgarom.asm:972
    mov ds, ax                                ; 8e d8                       ; 0xc04e8 vgarom.asm:973
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04ea vgarom.asm:974
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04ed vgarom.asm:975
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04ef vgarom.asm:976
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04f1 vgarom.asm:977
    mov byte [bx], al                         ; 88 07                       ; 0xc04f3 vgarom.asm:978
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04f5 vgarom.asm:979
    pop dx                                    ; 5a                          ; 0xc04f8 vgarom.asm:980
    pop bx                                    ; 5b                          ; 0xc04f9 vgarom.asm:981
    pop DS                                    ; 1f                          ; 0xc04fa vgarom.asm:982
    retn                                      ; c3                          ; 0xc04fb vgarom.asm:983
    push bx                                   ; 53                          ; 0xc04fc vgarom.asm:987
    push dx                                   ; 52                          ; 0xc04fd vgarom.asm:988
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04fe vgarom.asm:989
    and bl, 001h                              ; 80 e3 01                    ; 0xc0500 vgarom.asm:990
    xor bl, 001h                              ; 80 f3 01                    ; 0xc0503 vgarom.asm:991
    sal bl, 1                                 ; d0 e3                       ; 0xc0506 vgarom.asm:992
    mov dx, 003cch                            ; ba cc 03                    ; 0xc0508 vgarom.asm:993
    in AL, DX                                 ; ec                          ; 0xc050b vgarom.asm:994
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc050c vgarom.asm:995
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc050e vgarom.asm:996
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0510 vgarom.asm:997
    out DX, AL                                ; ee                          ; 0xc0513 vgarom.asm:998
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0514 vgarom.asm:999
    pop dx                                    ; 5a                          ; 0xc0517 vgarom.asm:1000
    pop bx                                    ; 5b                          ; 0xc0518 vgarom.asm:1001
    retn                                      ; c3                          ; 0xc0519 vgarom.asm:1002
    push DS                                   ; 1e                          ; 0xc051a vgarom.asm:1006
    push bx                                   ; 53                          ; 0xc051b vgarom.asm:1007
    push dx                                   ; 52                          ; 0xc051c vgarom.asm:1008
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc051d vgarom.asm:1009
    and dl, 001h                              ; 80 e2 01                    ; 0xc051f vgarom.asm:1010
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0522 vgarom.asm:1011
    sal dl, 1                                 ; d0 e2                       ; 0xc0525 vgarom.asm:1012
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0527 vgarom.asm:1013
    mov ds, ax                                ; 8e d8                       ; 0xc052a vgarom.asm:1014
    mov bx, 00089h                            ; bb 89 00                    ; 0xc052c vgarom.asm:1015
    mov al, byte [bx]                         ; 8a 07                       ; 0xc052f vgarom.asm:1016
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0531 vgarom.asm:1017
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0533 vgarom.asm:1018
    mov byte [bx], al                         ; 88 07                       ; 0xc0535 vgarom.asm:1019
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0537 vgarom.asm:1020
    pop dx                                    ; 5a                          ; 0xc053a vgarom.asm:1021
    pop bx                                    ; 5b                          ; 0xc053b vgarom.asm:1022
    pop DS                                    ; 1f                          ; 0xc053c vgarom.asm:1023
    retn                                      ; c3                          ; 0xc053d vgarom.asm:1024
    push DS                                   ; 1e                          ; 0xc053e vgarom.asm:1028
    push bx                                   ; 53                          ; 0xc053f vgarom.asm:1029
    push dx                                   ; 52                          ; 0xc0540 vgarom.asm:1030
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0541 vgarom.asm:1031
    and dl, 001h                              ; 80 e2 01                    ; 0xc0543 vgarom.asm:1032
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0546 vgarom.asm:1033
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0549 vgarom.asm:1034
    mov ds, ax                                ; 8e d8                       ; 0xc054c vgarom.asm:1035
    mov bx, 00089h                            ; bb 89 00                    ; 0xc054e vgarom.asm:1036
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0551 vgarom.asm:1037
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0553 vgarom.asm:1038
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0555 vgarom.asm:1039
    mov byte [bx], al                         ; 88 07                       ; 0xc0557 vgarom.asm:1040
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0559 vgarom.asm:1041
    pop dx                                    ; 5a                          ; 0xc055c vgarom.asm:1042
    pop bx                                    ; 5b                          ; 0xc055d vgarom.asm:1043
    pop DS                                    ; 1f                          ; 0xc055e vgarom.asm:1044
    retn                                      ; c3                          ; 0xc055f vgarom.asm:1045
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc0560 vgarom.asm:1050
    je short 00569h                           ; 74 05                       ; 0xc0562 vgarom.asm:1051
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0564 vgarom.asm:1052
    je short 0057eh                           ; 74 16                       ; 0xc0566 vgarom.asm:1053
    retn                                      ; c3                          ; 0xc0568 vgarom.asm:1057
    push DS                                   ; 1e                          ; 0xc0569 vgarom.asm:1059
    push ax                                   ; 50                          ; 0xc056a vgarom.asm:1060
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc056b vgarom.asm:1061
    mov ds, ax                                ; 8e d8                       ; 0xc056e vgarom.asm:1062
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0570 vgarom.asm:1063
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0573 vgarom.asm:1064
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0575 vgarom.asm:1065
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0577 vgarom.asm:1066
    pop ax                                    ; 58                          ; 0xc0579 vgarom.asm:1067
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc057a vgarom.asm:1068
    pop DS                                    ; 1f                          ; 0xc057c vgarom.asm:1069
    retn                                      ; c3                          ; 0xc057d vgarom.asm:1070
    push DS                                   ; 1e                          ; 0xc057e vgarom.asm:1072
    push ax                                   ; 50                          ; 0xc057f vgarom.asm:1073
    push bx                                   ; 53                          ; 0xc0580 vgarom.asm:1074
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0581 vgarom.asm:1075
    mov ds, ax                                ; 8e d8                       ; 0xc0584 vgarom.asm:1076
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0586 vgarom.asm:1077
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0588 vgarom.asm:1078
    mov byte [bx], al                         ; 88 07                       ; 0xc058b vgarom.asm:1079
    pop bx                                    ; 5b                          ; 0xc058d vgarom.asm:1089
    pop ax                                    ; 58                          ; 0xc058e vgarom.asm:1090
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc058f vgarom.asm:1091
    pop DS                                    ; 1f                          ; 0xc0591 vgarom.asm:1092
    retn                                      ; c3                          ; 0xc0592 vgarom.asm:1093
    times 0xd db 0
  ; disGetNextSymbol 0xc05a0 LB 0x3af -> off=0x0 cb=0000000000000007 uValue=00000000000c05a0 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc05a0 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc05a0 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc05a2 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc05a3 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc05a5 vberom.asm:72
    retn                                      ; c3                          ; 0xc05a6 vberom.asm:73
  ; disGetNextSymbol 0xc05a7 LB 0x3a8 -> off=0x0 cb=0000000000000043 uValue=00000000000c05a7 'do_in_ax_dx'
do_in_ax_dx:                                 ; 0xc05a7 LB 0x43
    in AL, DX                                 ; ec                          ; 0xc05a7 vberom.asm:76
    xchg ah, al                               ; 86 c4                       ; 0xc05a8 vberom.asm:77
    in AL, DX                                 ; ec                          ; 0xc05aa vberom.asm:78
    retn                                      ; c3                          ; 0xc05ab vberom.asm:79
    push ax                                   ; 50                          ; 0xc05ac vberom.asm:90
    push dx                                   ; 52                          ; 0xc05ad vberom.asm:91
    mov dx, 003dah                            ; ba da 03                    ; 0xc05ae vberom.asm:92
    in AL, DX                                 ; ec                          ; 0xc05b1 vberom.asm:94
    test AL, strict byte 008h                 ; a8 08                       ; 0xc05b2 vberom.asm:95
    je short 005b1h                           ; 74 fb                       ; 0xc05b4 vberom.asm:96
    pop dx                                    ; 5a                          ; 0xc05b6 vberom.asm:97
    pop ax                                    ; 58                          ; 0xc05b7 vberom.asm:98
    retn                                      ; c3                          ; 0xc05b8 vberom.asm:99
    push ax                                   ; 50                          ; 0xc05b9 vberom.asm:102
    push dx                                   ; 52                          ; 0xc05ba vberom.asm:103
    mov dx, 003dah                            ; ba da 03                    ; 0xc05bb vberom.asm:104
    in AL, DX                                 ; ec                          ; 0xc05be vberom.asm:106
    test AL, strict byte 008h                 ; a8 08                       ; 0xc05bf vberom.asm:107
    jne short 005beh                          ; 75 fb                       ; 0xc05c1 vberom.asm:108
    pop dx                                    ; 5a                          ; 0xc05c3 vberom.asm:109
    pop ax                                    ; 58                          ; 0xc05c4 vberom.asm:110
    retn                                      ; c3                          ; 0xc05c5 vberom.asm:111
    push dx                                   ; 52                          ; 0xc05c6 vberom.asm:116
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05c7 vberom.asm:117
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05ca vberom.asm:118
    call 005a0h                               ; e8 d0 ff                    ; 0xc05cd vberom.asm:119
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05d0 vberom.asm:120
    call 005a7h                               ; e8 d1 ff                    ; 0xc05d3 vberom.asm:121
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc05d6 vberom.asm:122
    jbe short 005e8h                          ; 76 0e                       ; 0xc05d8 vberom.asm:123
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc05da vberom.asm:124
    shr ah, 1                                 ; d0 ec                       ; 0xc05dc vberom.asm:128
    shr ah, 1                                 ; d0 ec                       ; 0xc05de vberom.asm:129
    shr ah, 1                                 ; d0 ec                       ; 0xc05e0 vberom.asm:130
    test AL, strict byte 007h                 ; a8 07                       ; 0xc05e2 vberom.asm:132
    je short 005e8h                           ; 74 02                       ; 0xc05e4 vberom.asm:133
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc05e6 vberom.asm:134
    pop dx                                    ; 5a                          ; 0xc05e8 vberom.asm:136
    retn                                      ; c3                          ; 0xc05e9 vberom.asm:137
  ; disGetNextSymbol 0xc05ea LB 0x365 -> off=0x0 cb=0000000000000026 uValue=00000000000c05ea '_dispi_get_max_bpp'
_dispi_get_max_bpp:                          ; 0xc05ea LB 0x26
    push dx                                   ; 52                          ; 0xc05ea vberom.asm:142
    push bx                                   ; 53                          ; 0xc05eb vberom.asm:143
    call 00624h                               ; e8 35 00                    ; 0xc05ec vberom.asm:144
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc05ef vberom.asm:145
    or ax, strict byte 00002h                 ; 83 c8 02                    ; 0xc05f1 vberom.asm:146
    call 00610h                               ; e8 19 00                    ; 0xc05f4 vberom.asm:147
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05f7 vberom.asm:148
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05fa vberom.asm:149
    call 005a0h                               ; e8 a0 ff                    ; 0xc05fd vberom.asm:150
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0600 vberom.asm:151
    call 005a7h                               ; e8 a1 ff                    ; 0xc0603 vberom.asm:152
    push ax                                   ; 50                          ; 0xc0606 vberom.asm:153
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0607 vberom.asm:154
    call 00610h                               ; e8 04 00                    ; 0xc0609 vberom.asm:155
    pop ax                                    ; 58                          ; 0xc060c vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc060d vberom.asm:157
    pop dx                                    ; 5a                          ; 0xc060e vberom.asm:158
    retn                                      ; c3                          ; 0xc060f vberom.asm:159
  ; disGetNextSymbol 0xc0610 LB 0x33f -> off=0x0 cb=0000000000000026 uValue=00000000000c0610 'dispi_set_enable_'
dispi_set_enable_:                           ; 0xc0610 LB 0x26
    push dx                                   ; 52                          ; 0xc0610 vberom.asm:162
    push ax                                   ; 50                          ; 0xc0611 vberom.asm:163
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0612 vberom.asm:164
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc0615 vberom.asm:165
    call 005a0h                               ; e8 85 ff                    ; 0xc0618 vberom.asm:166
    pop ax                                    ; 58                          ; 0xc061b vberom.asm:167
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc061c vberom.asm:168
    call 005a0h                               ; e8 7e ff                    ; 0xc061f vberom.asm:169
    pop dx                                    ; 5a                          ; 0xc0622 vberom.asm:170
    retn                                      ; c3                          ; 0xc0623 vberom.asm:171
    push dx                                   ; 52                          ; 0xc0624 vberom.asm:174
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0625 vberom.asm:175
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc0628 vberom.asm:176
    call 005a0h                               ; e8 72 ff                    ; 0xc062b vberom.asm:177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc062e vberom.asm:178
    call 005a7h                               ; e8 73 ff                    ; 0xc0631 vberom.asm:179
    pop dx                                    ; 5a                          ; 0xc0634 vberom.asm:180
    retn                                      ; c3                          ; 0xc0635 vberom.asm:181
  ; disGetNextSymbol 0xc0636 LB 0x319 -> off=0x0 cb=0000000000000026 uValue=00000000000c0636 'dispi_set_bank_'
dispi_set_bank_:                             ; 0xc0636 LB 0x26
    push dx                                   ; 52                          ; 0xc0636 vberom.asm:184
    push ax                                   ; 50                          ; 0xc0637 vberom.asm:185
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0638 vberom.asm:186
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc063b vberom.asm:187
    call 005a0h                               ; e8 5f ff                    ; 0xc063e vberom.asm:188
    pop ax                                    ; 58                          ; 0xc0641 vberom.asm:189
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0642 vberom.asm:190
    call 005a0h                               ; e8 58 ff                    ; 0xc0645 vberom.asm:191
    pop dx                                    ; 5a                          ; 0xc0648 vberom.asm:192
    retn                                      ; c3                          ; 0xc0649 vberom.asm:193
    push dx                                   ; 52                          ; 0xc064a vberom.asm:196
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc064b vberom.asm:197
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc064e vberom.asm:198
    call 005a0h                               ; e8 4c ff                    ; 0xc0651 vberom.asm:199
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0654 vberom.asm:200
    call 005a7h                               ; e8 4d ff                    ; 0xc0657 vberom.asm:201
    pop dx                                    ; 5a                          ; 0xc065a vberom.asm:202
    retn                                      ; c3                          ; 0xc065b vberom.asm:203
  ; disGetNextSymbol 0xc065c LB 0x2f3 -> off=0x0 cb=00000000000000ac uValue=00000000000c065c '_dispi_set_bank_farcall'
_dispi_set_bank_farcall:                     ; 0xc065c LB 0xac
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc065c vberom.asm:206
    je short 00686h                           ; 74 24                       ; 0xc0660 vberom.asm:207
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db                     ; 0xc0662 vberom.asm:208
    jne short 00698h                          ; 75 32                       ; 0xc0664 vberom.asm:209
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0666 vberom.asm:210
    push dx                                   ; 52                          ; 0xc0668 vberom.asm:211
    push ax                                   ; 50                          ; 0xc0669 vberom.asm:212
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc066a vberom.asm:213
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc066d vberom.asm:214
    call 005a0h                               ; e8 2d ff                    ; 0xc0670 vberom.asm:215
    pop ax                                    ; 58                          ; 0xc0673 vberom.asm:216
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0674 vberom.asm:217
    call 005a0h                               ; e8 26 ff                    ; 0xc0677 vberom.asm:218
    call 005a7h                               ; e8 2a ff                    ; 0xc067a vberom.asm:219
    pop dx                                    ; 5a                          ; 0xc067d vberom.asm:220
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0                     ; 0xc067e vberom.asm:221
    jne short 00698h                          ; 75 16                       ; 0xc0680 vberom.asm:222
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0682 vberom.asm:223
    retf                                      ; cb                          ; 0xc0685 vberom.asm:224
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0686 vberom.asm:226
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0689 vberom.asm:227
    call 005a0h                               ; e8 11 ff                    ; 0xc068c vberom.asm:228
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc068f vberom.asm:229
    call 005a7h                               ; e8 12 ff                    ; 0xc0692 vberom.asm:230
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0695 vberom.asm:231
    retf                                      ; cb                          ; 0xc0697 vberom.asm:232
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0698 vberom.asm:234
    retf                                      ; cb                          ; 0xc069b vberom.asm:235
    push dx                                   ; 52                          ; 0xc069c vberom.asm:238
    push ax                                   ; 50                          ; 0xc069d vberom.asm:239
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc069e vberom.asm:240
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc06a1 vberom.asm:241
    call 005a0h                               ; e8 f9 fe                    ; 0xc06a4 vberom.asm:242
    pop ax                                    ; 58                          ; 0xc06a7 vberom.asm:243
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06a8 vberom.asm:244
    call 005a0h                               ; e8 f2 fe                    ; 0xc06ab vberom.asm:245
    pop dx                                    ; 5a                          ; 0xc06ae vberom.asm:246
    retn                                      ; c3                          ; 0xc06af vberom.asm:247
    push dx                                   ; 52                          ; 0xc06b0 vberom.asm:250
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06b1 vberom.asm:251
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc06b4 vberom.asm:252
    call 005a0h                               ; e8 e6 fe                    ; 0xc06b7 vberom.asm:253
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06ba vberom.asm:254
    call 005a7h                               ; e8 e7 fe                    ; 0xc06bd vberom.asm:255
    pop dx                                    ; 5a                          ; 0xc06c0 vberom.asm:256
    retn                                      ; c3                          ; 0xc06c1 vberom.asm:257
    push dx                                   ; 52                          ; 0xc06c2 vberom.asm:260
    push ax                                   ; 50                          ; 0xc06c3 vberom.asm:261
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06c4 vberom.asm:262
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06c7 vberom.asm:263
    call 005a0h                               ; e8 d3 fe                    ; 0xc06ca vberom.asm:264
    pop ax                                    ; 58                          ; 0xc06cd vberom.asm:265
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06ce vberom.asm:266
    call 005a0h                               ; e8 cc fe                    ; 0xc06d1 vberom.asm:267
    pop dx                                    ; 5a                          ; 0xc06d4 vberom.asm:268
    retn                                      ; c3                          ; 0xc06d5 vberom.asm:269
    push dx                                   ; 52                          ; 0xc06d6 vberom.asm:272
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06d7 vberom.asm:273
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06da vberom.asm:274
    call 005a0h                               ; e8 c0 fe                    ; 0xc06dd vberom.asm:275
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06e0 vberom.asm:276
    call 005a7h                               ; e8 c1 fe                    ; 0xc06e3 vberom.asm:277
    pop dx                                    ; 5a                          ; 0xc06e6 vberom.asm:278
    retn                                      ; c3                          ; 0xc06e7 vberom.asm:279
    push ax                                   ; 50                          ; 0xc06e8 vberom.asm:282
    push bx                                   ; 53                          ; 0xc06e9 vberom.asm:283
    push dx                                   ; 52                          ; 0xc06ea vberom.asm:284
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc06eb vberom.asm:285
    call 005c6h                               ; e8 d6 fe                    ; 0xc06ed vberom.asm:286
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc06f0 vberom.asm:287
    jnbe short 006f6h                         ; 77 02                       ; 0xc06f2 vberom.asm:288
    shr bx, 1                                 ; d1 eb                       ; 0xc06f4 vberom.asm:289
    shr bx, 1                                 ; d1 eb                       ; 0xc06f6 vberom.asm:294
    shr bx, 1                                 ; d1 eb                       ; 0xc06f8 vberom.asm:295
    shr bx, 1                                 ; d1 eb                       ; 0xc06fa vberom.asm:296
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06fc vberom.asm:298
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc06ff vberom.asm:299
    mov AL, strict byte 013h                  ; b0 13                       ; 0xc0701 vberom.asm:300
    out DX, ax                                ; ef                          ; 0xc0703 vberom.asm:301
    pop dx                                    ; 5a                          ; 0xc0704 vberom.asm:302
    pop bx                                    ; 5b                          ; 0xc0705 vberom.asm:303
    pop ax                                    ; 58                          ; 0xc0706 vberom.asm:304
    retn                                      ; c3                          ; 0xc0707 vberom.asm:305
  ; disGetNextSymbol 0xc0708 LB 0x247 -> off=0x0 cb=00000000000000f0 uValue=00000000000c0708 '_vga_compat_setup'
_vga_compat_setup:                           ; 0xc0708 LB 0xf0
    push ax                                   ; 50                          ; 0xc0708 vberom.asm:308
    push dx                                   ; 52                          ; 0xc0709 vberom.asm:309
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc070a vberom.asm:312
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc070d vberom.asm:313
    call 005a0h                               ; e8 8d fe                    ; 0xc0710 vberom.asm:314
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0713 vberom.asm:315
    call 005a7h                               ; e8 8e fe                    ; 0xc0716 vberom.asm:316
    push ax                                   ; 50                          ; 0xc0719 vberom.asm:317
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc071a vberom.asm:318
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc071d vberom.asm:319
    out DX, ax                                ; ef                          ; 0xc0720 vberom.asm:320
    pop ax                                    ; 58                          ; 0xc0721 vberom.asm:321
    push ax                                   ; 50                          ; 0xc0722 vberom.asm:322
    shr ax, 1                                 ; d1 e8                       ; 0xc0723 vberom.asm:326
    shr ax, 1                                 ; d1 e8                       ; 0xc0725 vberom.asm:327
    shr ax, 1                                 ; d1 e8                       ; 0xc0727 vberom.asm:328
    dec ax                                    ; 48                          ; 0xc0729 vberom.asm:330
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc072a vberom.asm:331
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc072c vberom.asm:332
    out DX, ax                                ; ef                          ; 0xc072e vberom.asm:333
    pop ax                                    ; 58                          ; 0xc072f vberom.asm:334
    call 006e8h                               ; e8 b5 ff                    ; 0xc0730 vberom.asm:335
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0733 vberom.asm:338
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc0736 vberom.asm:339
    call 005a0h                               ; e8 64 fe                    ; 0xc0739 vberom.asm:340
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc073c vberom.asm:341
    call 005a7h                               ; e8 65 fe                    ; 0xc073f vberom.asm:342
    dec ax                                    ; 48                          ; 0xc0742 vberom.asm:343
    push ax                                   ; 50                          ; 0xc0743 vberom.asm:344
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0744 vberom.asm:345
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0747 vberom.asm:346
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc0749 vberom.asm:347
    out DX, ax                                ; ef                          ; 0xc074b vberom.asm:348
    pop ax                                    ; 58                          ; 0xc074c vberom.asm:349
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc074d vberom.asm:350
    out DX, AL                                ; ee                          ; 0xc074f vberom.asm:351
    inc dx                                    ; 42                          ; 0xc0750 vberom.asm:352
    in AL, DX                                 ; ec                          ; 0xc0751 vberom.asm:353
    and AL, strict byte 0bdh                  ; 24 bd                       ; 0xc0752 vberom.asm:354
    test ah, 001h                             ; f6 c4 01                    ; 0xc0754 vberom.asm:355
    je short 0075bh                           ; 74 02                       ; 0xc0757 vberom.asm:356
    or AL, strict byte 002h                   ; 0c 02                       ; 0xc0759 vberom.asm:357
    test ah, 002h                             ; f6 c4 02                    ; 0xc075b vberom.asm:359
    je short 00762h                           ; 74 02                       ; 0xc075e vberom.asm:360
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0760 vberom.asm:361
    out DX, AL                                ; ee                          ; 0xc0762 vberom.asm:363
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0763 vberom.asm:366
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0766 vberom.asm:367
    out DX, AL                                ; ee                          ; 0xc0769 vberom.asm:368
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc076a vberom.asm:369
    in AL, DX                                 ; ec                          ; 0xc076d vberom.asm:370
    and AL, strict byte 060h                  ; 24 60                       ; 0xc076e vberom.asm:371
    out DX, AL                                ; ee                          ; 0xc0770 vberom.asm:372
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0771 vberom.asm:373
    mov AL, strict byte 017h                  ; b0 17                       ; 0xc0774 vberom.asm:374
    out DX, AL                                ; ee                          ; 0xc0776 vberom.asm:375
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0777 vberom.asm:376
    in AL, DX                                 ; ec                          ; 0xc077a vberom.asm:377
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc077b vberom.asm:378
    out DX, AL                                ; ee                          ; 0xc077d vberom.asm:379
    mov dx, 003dah                            ; ba da 03                    ; 0xc077e vberom.asm:380
    in AL, DX                                 ; ec                          ; 0xc0781 vberom.asm:381
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0782 vberom.asm:382
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0785 vberom.asm:383
    out DX, AL                                ; ee                          ; 0xc0787 vberom.asm:384
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0788 vberom.asm:385
    in AL, DX                                 ; ec                          ; 0xc078b vberom.asm:386
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc078c vberom.asm:387
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc078e vberom.asm:388
    out DX, AL                                ; ee                          ; 0xc0791 vberom.asm:389
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0792 vberom.asm:390
    out DX, AL                                ; ee                          ; 0xc0794 vberom.asm:391
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0795 vberom.asm:392
    mov ax, 00506h                            ; b8 06 05                    ; 0xc0798 vberom.asm:393
    out DX, ax                                ; ef                          ; 0xc079b vberom.asm:394
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc079c vberom.asm:395
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc079f vberom.asm:396
    out DX, ax                                ; ef                          ; 0xc07a2 vberom.asm:397
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc07a3 vberom.asm:400
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc07a6 vberom.asm:401
    call 005a0h                               ; e8 f4 fd                    ; 0xc07a9 vberom.asm:402
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc07ac vberom.asm:403
    call 005a7h                               ; e8 f5 fd                    ; 0xc07af vberom.asm:404
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc07b2 vberom.asm:405
    jc short 007f6h                           ; 72 40                       ; 0xc07b4 vberom.asm:406
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc07b6 vberom.asm:407
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc07b9 vberom.asm:408
    out DX, AL                                ; ee                          ; 0xc07bb vberom.asm:409
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc07bc vberom.asm:410
    in AL, DX                                 ; ec                          ; 0xc07bf vberom.asm:411
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07c0 vberom.asm:412
    out DX, AL                                ; ee                          ; 0xc07c2 vberom.asm:413
    mov dx, 003dah                            ; ba da 03                    ; 0xc07c3 vberom.asm:414
    in AL, DX                                 ; ec                          ; 0xc07c6 vberom.asm:415
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc07c7 vberom.asm:416
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc07ca vberom.asm:417
    out DX, AL                                ; ee                          ; 0xc07cc vberom.asm:418
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc07cd vberom.asm:419
    in AL, DX                                 ; ec                          ; 0xc07d0 vberom.asm:420
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07d1 vberom.asm:421
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc07d3 vberom.asm:422
    out DX, AL                                ; ee                          ; 0xc07d6 vberom.asm:423
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc07d7 vberom.asm:424
    out DX, AL                                ; ee                          ; 0xc07d9 vberom.asm:425
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc07da vberom.asm:426
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc07dd vberom.asm:427
    out DX, AL                                ; ee                          ; 0xc07df vberom.asm:428
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc07e0 vberom.asm:429
    in AL, DX                                 ; ec                          ; 0xc07e3 vberom.asm:430
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc07e4 vberom.asm:431
    out DX, AL                                ; ee                          ; 0xc07e6 vberom.asm:432
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc07e7 vberom.asm:433
    mov AL, strict byte 005h                  ; b0 05                       ; 0xc07ea vberom.asm:434
    out DX, AL                                ; ee                          ; 0xc07ec vberom.asm:435
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc07ed vberom.asm:436
    in AL, DX                                 ; ec                          ; 0xc07f0 vberom.asm:437
    and AL, strict byte 09fh                  ; 24 9f                       ; 0xc07f1 vberom.asm:438
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07f3 vberom.asm:439
    out DX, AL                                ; ee                          ; 0xc07f5 vberom.asm:440
    pop dx                                    ; 5a                          ; 0xc07f6 vberom.asm:443
    pop ax                                    ; 58                          ; 0xc07f7 vberom.asm:444
  ; disGetNextSymbol 0xc07f8 LB 0x157 -> off=0x0 cb=0000000000000013 uValue=00000000000c07f8 '_vbe_has_vbe_display'
_vbe_has_vbe_display:                        ; 0xc07f8 LB 0x13
    push DS                                   ; 1e                          ; 0xc07f8 vberom.asm:450
    push bx                                   ; 53                          ; 0xc07f9 vberom.asm:451
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07fa vberom.asm:452
    mov ds, ax                                ; 8e d8                       ; 0xc07fd vberom.asm:453
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc07ff vberom.asm:454
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0802 vberom.asm:455
    and AL, strict byte 001h                  ; 24 01                       ; 0xc0804 vberom.asm:456
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0806 vberom.asm:457
    pop bx                                    ; 5b                          ; 0xc0808 vberom.asm:458
    pop DS                                    ; 1f                          ; 0xc0809 vberom.asm:459
    retn                                      ; c3                          ; 0xc080a vberom.asm:460
  ; disGetNextSymbol 0xc080b LB 0x144 -> off=0x0 cb=0000000000000025 uValue=00000000000c080b 'vbe_biosfn_return_current_mode'
vbe_biosfn_return_current_mode:              ; 0xc080b LB 0x25
    push DS                                   ; 1e                          ; 0xc080b vberom.asm:473
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc080c vberom.asm:474
    mov ds, ax                                ; 8e d8                       ; 0xc080f vberom.asm:475
    call 00624h                               ; e8 10 fe                    ; 0xc0811 vberom.asm:476
    and ax, strict byte 00001h                ; 83 e0 01                    ; 0xc0814 vberom.asm:477
    je short 00822h                           ; 74 09                       ; 0xc0817 vberom.asm:478
    mov bx, 000bah                            ; bb ba 00                    ; 0xc0819 vberom.asm:479
    mov ax, word [bx]                         ; 8b 07                       ; 0xc081c vberom.asm:480
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc081e vberom.asm:481
    jne short 0082bh                          ; 75 09                       ; 0xc0820 vberom.asm:482
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0822 vberom.asm:484
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0825 vberom.asm:485
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0827 vberom.asm:486
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0829 vberom.asm:487
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc082b vberom.asm:489
    pop DS                                    ; 1f                          ; 0xc082e vberom.asm:490
    retn                                      ; c3                          ; 0xc082f vberom.asm:491
  ; disGetNextSymbol 0xc0830 LB 0x11f -> off=0x0 cb=000000000000002d uValue=00000000000c0830 'vbe_biosfn_display_window_control'
vbe_biosfn_display_window_control:           ; 0xc0830 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc0830 vberom.asm:515
    jne short 00859h                          ; 75 24                       ; 0xc0833 vberom.asm:516
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc0835 vberom.asm:517
    je short 00850h                           ; 74 16                       ; 0xc0838 vberom.asm:518
    jc short 00840h                           ; 72 04                       ; 0xc083a vberom.asm:519
    mov ax, 00100h                            ; b8 00 01                    ; 0xc083c vberom.asm:520
    retn                                      ; c3                          ; 0xc083f vberom.asm:521
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0840 vberom.asm:523
    call 00636h                               ; e8 f1 fd                    ; 0xc0842 vberom.asm:524
    call 0064ah                               ; e8 02 fe                    ; 0xc0845 vberom.asm:525
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2                     ; 0xc0848 vberom.asm:526
    jne short 00859h                          ; 75 0d                       ; 0xc084a vberom.asm:527
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc084c vberom.asm:528
    retn                                      ; c3                          ; 0xc084f vberom.asm:529
    call 0064ah                               ; e8 f7 fd                    ; 0xc0850 vberom.asm:531
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0853 vberom.asm:532
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0855 vberom.asm:533
    retn                                      ; c3                          ; 0xc0858 vberom.asm:534
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0859 vberom.asm:536
    retn                                      ; c3                          ; 0xc085c vberom.asm:537
  ; disGetNextSymbol 0xc085d LB 0xf2 -> off=0x0 cb=0000000000000034 uValue=00000000000c085d 'vbe_biosfn_set_get_display_start'
vbe_biosfn_set_get_display_start:            ; 0xc085d LB 0x34
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc085d vberom.asm:577
    je short 0086dh                           ; 74 0b                       ; 0xc0860 vberom.asm:578
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0862 vberom.asm:579
    je short 00881h                           ; 74 1a                       ; 0xc0865 vberom.asm:580
    jc short 00873h                           ; 72 0a                       ; 0xc0867 vberom.asm:581
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0869 vberom.asm:582
    retn                                      ; c3                          ; 0xc086c vberom.asm:583
    call 005b9h                               ; e8 49 fd                    ; 0xc086d vberom.asm:585
    call 005ach                               ; e8 39 fd                    ; 0xc0870 vberom.asm:586
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc0873 vberom.asm:588
    call 0069ch                               ; e8 24 fe                    ; 0xc0875 vberom.asm:589
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0878 vberom.asm:590
    call 006c2h                               ; e8 45 fe                    ; 0xc087a vberom.asm:591
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc087d vberom.asm:592
    retn                                      ; c3                          ; 0xc0880 vberom.asm:593
    call 006b0h                               ; e8 2c fe                    ; 0xc0881 vberom.asm:595
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8                     ; 0xc0884 vberom.asm:596
    call 006d6h                               ; e8 4d fe                    ; 0xc0886 vberom.asm:597
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0889 vberom.asm:598
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc088b vberom.asm:599
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc088d vberom.asm:600
    retn                                      ; c3                          ; 0xc0890 vberom.asm:601
  ; disGetNextSymbol 0xc0891 LB 0xbe -> off=0x0 cb=0000000000000037 uValue=00000000000c0891 'vbe_biosfn_set_get_dac_palette_format'
vbe_biosfn_set_get_dac_palette_format:       ; 0xc0891 LB 0x37
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0891 vberom.asm:616
    je short 008b4h                           ; 74 1e                       ; 0xc0894 vberom.asm:617
    jc short 0089ch                           ; 72 04                       ; 0xc0896 vberom.asm:618
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0898 vberom.asm:619
    retn                                      ; c3                          ; 0xc089b vberom.asm:620
    call 00624h                               ; e8 85 fd                    ; 0xc089c vberom.asm:622
    cmp bh, 006h                              ; 80 ff 06                    ; 0xc089f vberom.asm:623
    je short 008aeh                           ; 74 0a                       ; 0xc08a2 vberom.asm:624
    cmp bh, 008h                              ; 80 ff 08                    ; 0xc08a4 vberom.asm:625
    jne short 008c4h                          ; 75 1b                       ; 0xc08a7 vberom.asm:626
    or ax, strict byte 00020h                 ; 83 c8 20                    ; 0xc08a9 vberom.asm:627
    jne short 008b1h                          ; 75 03                       ; 0xc08ac vberom.asm:628
    and ax, strict byte 0ffdfh                ; 83 e0 df                    ; 0xc08ae vberom.asm:630
    call 00610h                               ; e8 5c fd                    ; 0xc08b1 vberom.asm:632
    mov BH, strict byte 006h                  ; b7 06                       ; 0xc08b4 vberom.asm:634
    call 00624h                               ; e8 6b fd                    ; 0xc08b6 vberom.asm:635
    and ax, strict byte 00020h                ; 83 e0 20                    ; 0xc08b9 vberom.asm:636
    je short 008c0h                           ; 74 02                       ; 0xc08bc vberom.asm:637
    mov BH, strict byte 008h                  ; b7 08                       ; 0xc08be vberom.asm:638
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08c0 vberom.asm:640
    retn                                      ; c3                          ; 0xc08c3 vberom.asm:641
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08c4 vberom.asm:643
    retn                                      ; c3                          ; 0xc08c7 vberom.asm:644
  ; disGetNextSymbol 0xc08c8 LB 0x87 -> off=0x0 cb=0000000000000073 uValue=00000000000c08c8 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc08c8 LB 0x73
    test bl, bl                               ; 84 db                       ; 0xc08c8 vberom.asm:683
    je short 008dbh                           ; 74 0f                       ; 0xc08ca vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc08cc vberom.asm:685
    je short 00909h                           ; 74 38                       ; 0xc08cf vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc08d1 vberom.asm:687
    jbe short 00937h                          ; 76 61                       ; 0xc08d4 vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc08d6 vberom.asm:689
    jne short 00933h                          ; 75 58                       ; 0xc08d9 vberom.asm:690
    push ax                                   ; 50                          ; 0xc08db vberom.asm:145
    push cx                                   ; 51                          ; 0xc08dc vberom.asm:146
    push dx                                   ; 52                          ; 0xc08dd vberom.asm:147
    push bx                                   ; 53                          ; 0xc08de vberom.asm:148
    push sp                                   ; 54                          ; 0xc08df vberom.asm:149
    push bp                                   ; 55                          ; 0xc08e0 vberom.asm:150
    push si                                   ; 56                          ; 0xc08e1 vberom.asm:151
    push di                                   ; 57                          ; 0xc08e2 vberom.asm:152
    push DS                                   ; 1e                          ; 0xc08e3 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08e4 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08e5 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08e6 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08e8 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08eb vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08ec vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08ed vberom.asm:703
    lodsw                                     ; ad                          ; 0xc08ef vberom.asm:714
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc08f0 vberom.asm:715
    lodsw                                     ; ad                          ; 0xc08f2 vberom.asm:716
    out DX, AL                                ; ee                          ; 0xc08f3 vberom.asm:717
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc08f4 vberom.asm:718
    out DX, AL                                ; ee                          ; 0xc08f6 vberom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc08f7 vberom.asm:720
    out DX, AL                                ; ee                          ; 0xc08f9 vberom.asm:721
    loop 008efh                               ; e2 f3                       ; 0xc08fa vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08fc vberom.asm:724
    pop di                                    ; 5f                          ; 0xc08fd vberom.asm:164
    pop si                                    ; 5e                          ; 0xc08fe vberom.asm:165
    pop bp                                    ; 5d                          ; 0xc08ff vberom.asm:166
    pop bx                                    ; 5b                          ; 0xc0900 vberom.asm:167
    pop bx                                    ; 5b                          ; 0xc0901 vberom.asm:168
    pop dx                                    ; 5a                          ; 0xc0902 vberom.asm:169
    pop cx                                    ; 59                          ; 0xc0903 vberom.asm:170
    pop ax                                    ; 58                          ; 0xc0904 vberom.asm:171
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0905 vberom.asm:727
    retn                                      ; c3                          ; 0xc0908 vberom.asm:728
    push ax                                   ; 50                          ; 0xc0909 vberom.asm:145
    push cx                                   ; 51                          ; 0xc090a vberom.asm:146
    push dx                                   ; 52                          ; 0xc090b vberom.asm:147
    push bx                                   ; 53                          ; 0xc090c vberom.asm:148
    push sp                                   ; 54                          ; 0xc090d vberom.asm:149
    push bp                                   ; 55                          ; 0xc090e vberom.asm:150
    push si                                   ; 56                          ; 0xc090f vberom.asm:151
    push di                                   ; 57                          ; 0xc0910 vberom.asm:152
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc0911 vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0913 vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc0916 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc0917 vberom.asm:735
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db                     ; 0xc091a vberom.asm:746
    in AL, DX                                 ; ec                          ; 0xc091c vberom.asm:748
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc091d vberom.asm:749
    in AL, DX                                 ; ec                          ; 0xc091f vberom.asm:750
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0920 vberom.asm:751
    in AL, DX                                 ; ec                          ; 0xc0922 vberom.asm:752
    stosw                                     ; ab                          ; 0xc0923 vberom.asm:753
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0924 vberom.asm:754
    stosw                                     ; ab                          ; 0xc0926 vberom.asm:755
    loop 0091ch                               ; e2 f3                       ; 0xc0927 vberom.asm:757
    pop di                                    ; 5f                          ; 0xc0929 vberom.asm:164
    pop si                                    ; 5e                          ; 0xc092a vberom.asm:165
    pop bp                                    ; 5d                          ; 0xc092b vberom.asm:166
    pop bx                                    ; 5b                          ; 0xc092c vberom.asm:167
    pop bx                                    ; 5b                          ; 0xc092d vberom.asm:168
    pop dx                                    ; 5a                          ; 0xc092e vberom.asm:169
    pop cx                                    ; 59                          ; 0xc092f vberom.asm:170
    pop ax                                    ; 58                          ; 0xc0930 vberom.asm:171
    jmp short 00905h                          ; eb d2                       ; 0xc0931 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0933 vberom.asm:762
    retn                                      ; c3                          ; 0xc0936 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc0937 vberom.asm:765
    retn                                      ; c3                          ; 0xc093a vberom.asm:766
  ; disGetNextSymbol 0xc093b LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c093b 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc093b LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc093b vberom.asm:780
    jne short 0094bh                          ; 75 0c                       ; 0xc093d vberom.asm:781
    push CS                                   ; 0e                          ; 0xc093f vberom.asm:782
    pop ES                                    ; 07                          ; 0xc0940 vberom.asm:783
    mov di, 04640h                            ; bf 40 46                    ; 0xc0941 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc0944 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0947 vberom.asm:786
    retn                                      ; c3                          ; 0xc094a vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc094b vberom.asm:789
    retn                                      ; c3                          ; 0xc094e vberom.asm:790

  ; Padding 0xa1 bytes at 0xc094f
  times 161 db 0

section _TEXT progbits vstart=0x9f0 align=1 ; size=0x3c1d class=CODE group=AUTO
  ; disGetNextSymbol 0xc09f0 LB 0x3c1d -> off=0x0 cb=000000000000001c uValue=00000000000c09f0 'set_int_vector'
set_int_vector:                              ; 0xc09f0 LB 0x1c
    push dx                                   ; 52                          ; 0xc09f0 vgabios.c:88
    push bp                                   ; 55                          ; 0xc09f1
    mov bp, sp                                ; 89 e5                       ; 0xc09f2
    mov dx, bx                                ; 89 da                       ; 0xc09f4
    mov bl, al                                ; 88 c3                       ; 0xc09f6 vgabios.c:92
    xor bh, bh                                ; 30 ff                       ; 0xc09f8
    sal bx, 1                                 ; d1 e3                       ; 0xc09fa
    sal bx, 1                                 ; d1 e3                       ; 0xc09fc
    xor ax, ax                                ; 31 c0                       ; 0xc09fe
    mov es, ax                                ; 8e c0                       ; 0xc0a00
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a02
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0a05
    pop bp                                    ; 5d                          ; 0xc0a09 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc0a0a
    retn                                      ; c3                          ; 0xc0a0b
  ; disGetNextSymbol 0xc0a0c LB 0x3c01 -> off=0x0 cb=000000000000001c uValue=00000000000c0a0c 'init_vga_card'
init_vga_card:                               ; 0xc0a0c LB 0x1c
    push bp                                   ; 55                          ; 0xc0a0c vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc0a0d
    push dx                                   ; 52                          ; 0xc0a0f
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a10 vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a12
    out DX, AL                                ; ee                          ; 0xc0a15
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a16 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a18
    out DX, AL                                ; ee                          ; 0xc0a1b
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a1c vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a1e
    out DX, AL                                ; ee                          ; 0xc0a21
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a22 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc0a25
    pop bp                                    ; 5d                          ; 0xc0a26
    retn                                      ; c3                          ; 0xc0a27
  ; disGetNextSymbol 0xc0a28 LB 0x3be5 -> off=0x0 cb=000000000000003e uValue=00000000000c0a28 'init_bios_area'
init_bios_area:                              ; 0xc0a28 LB 0x3e
    push bx                                   ; 53                          ; 0xc0a28 vgabios.c:222
    push bp                                   ; 55                          ; 0xc0a29
    mov bp, sp                                ; 89 e5                       ; 0xc0a2a
    xor bx, bx                                ; 31 db                       ; 0xc0a2c vgabios.c:226
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a2e
    mov es, ax                                ; 8e c0                       ; 0xc0a31
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a33 vgabios.c:229
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a37
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a39
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a3b
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a3f vgabios.c:233
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a45 vgabios.c:235
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a4c vgabios.c:239
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a52 vgabios.c:241
    mov word [es:bx+000a8h], 05550h           ; 26 c7 87 a8 00 50 55        ; 0xc0a57 vgabios.c:243
    mov [es:bx+000aah], ds                    ; 26 8c 9f aa 00              ; 0xc0a5e
    pop bp                                    ; 5d                          ; 0xc0a63 vgabios.c:244
    pop bx                                    ; 5b                          ; 0xc0a64
    retn                                      ; c3                          ; 0xc0a65
  ; disGetNextSymbol 0xc0a66 LB 0x3ba7 -> off=0x0 cb=0000000000000031 uValue=00000000000c0a66 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a66 LB 0x31
    inc bp                                    ; 45                          ; 0xc0a66 vgabios.c:251
    push bp                                   ; 55                          ; 0xc0a67
    mov bp, sp                                ; 89 e5                       ; 0xc0a68
    call 00a0ch                               ; e8 9f ff                    ; 0xc0a6a vgabios.c:253
    call 00a28h                               ; e8 b8 ff                    ; 0xc0a6d vgabios.c:254
    call 03f7eh                               ; e8 0b 35                    ; 0xc0a70 vgabios.c:256
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a73 vgabios.c:258
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a76
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a79
    call 009f0h                               ; e8 71 ff                    ; 0xc0a7c
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a7f vgabios.c:259
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a82
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a85
    call 009f0h                               ; e8 65 ff                    ; 0xc0a88
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a8b vgabios.c:285
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a8e
    int 010h                                  ; cd 10                       ; 0xc0a90
    mov sp, bp                                ; 89 ec                       ; 0xc0a92 vgabios.c:288
    pop bp                                    ; 5d                          ; 0xc0a94
    dec bp                                    ; 4d                          ; 0xc0a95
    retf                                      ; cb                          ; 0xc0a96
  ; disGetNextSymbol 0xc0a97 LB 0x3b76 -> off=0x0 cb=0000000000000040 uValue=00000000000c0a97 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a97 LB 0x40
    push si                                   ; 56                          ; 0xc0a97 vgabios.c:357
    push di                                   ; 57                          ; 0xc0a98
    push bp                                   ; 55                          ; 0xc0a99
    mov bp, sp                                ; 89 e5                       ; 0xc0a9a
    mov si, dx                                ; 89 d6                       ; 0xc0a9c
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a9e vgabios.c:359
    jbe short 00ab0h                          ; 76 0e                       ; 0xc0aa0
    push SS                                   ; 16                          ; 0xc0aa2 vgabios.c:360
    pop ES                                    ; 07                          ; 0xc0aa3
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0aa4
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0aa9 vgabios.c:361
    jmp short 00ad3h                          ; eb 23                       ; 0xc0aae vgabios.c:362
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0ab0 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0ab3
    mov es, dx                                ; 8e c2                       ; 0xc0ab6
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0ab8
    push SS                                   ; 16                          ; 0xc0abb vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0abc
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0abd
    xor ah, ah                                ; 30 e4                       ; 0xc0ac0 vgabios.c:365
    mov si, ax                                ; 89 c6                       ; 0xc0ac2
    sal si, 1                                 ; d1 e6                       ; 0xc0ac4
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0ac6
    mov es, dx                                ; 8e c2                       ; 0xc0ac9 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0acb
    push SS                                   ; 16                          ; 0xc0ace vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0acf
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ad0
    pop bp                                    ; 5d                          ; 0xc0ad3 vgabios.c:367
    pop di                                    ; 5f                          ; 0xc0ad4
    pop si                                    ; 5e                          ; 0xc0ad5
    retn                                      ; c3                          ; 0xc0ad6
  ; disGetNextSymbol 0xc0ad7 LB 0x3b36 -> off=0x0 cb=000000000000005e uValue=00000000000c0ad7 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0ad7 LB 0x5e
    push bp                                   ; 55                          ; 0xc0ad7 vgabios.c:370
    mov bp, sp                                ; 89 e5                       ; 0xc0ad8
    push si                                   ; 56                          ; 0xc0ada
    push di                                   ; 57                          ; 0xc0adb
    push ax                                   ; 50                          ; 0xc0adc
    push ax                                   ; 50                          ; 0xc0add
    push dx                                   ; 52                          ; 0xc0ade
    push bx                                   ; 53                          ; 0xc0adf
    mov bl, cl                                ; 88 cb                       ; 0xc0ae0
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0ae2 vgabios.c:372
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0ae7 vgabios.c:374
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0aea
    je short 00b29h                           ; 74 39                       ; 0xc0aee
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0af0 vgabios.c:375
    xor ch, ch                                ; 30 ed                       ; 0xc0af3
    mov dx, ss                                ; 8c d2                       ; 0xc0af5
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0af7
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0afa
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0afd
    push DS                                   ; 1e                          ; 0xc0b00
    mov ds, dx                                ; 8e da                       ; 0xc0b01
    rep cmpsb                                 ; f3 a6                       ; 0xc0b03
    pop DS                                    ; 1f                          ; 0xc0b05
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0b06
    je short 00b0dh                           ; 74 02                       ; 0xc0b09
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0b0b
    test ax, ax                               ; 85 c0                       ; 0xc0b0d
    jne short 00b1dh                          ; 75 0c                       ; 0xc0b0f
    mov al, bl                                ; 88 d8                       ; 0xc0b11 vgabios.c:376
    xor ah, ah                                ; 30 e4                       ; 0xc0b13
    or ah, 080h                               ; 80 cc 80                    ; 0xc0b15
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0b18
    jmp short 00b29h                          ; eb 0c                       ; 0xc0b1b vgabios.c:377
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0b1d vgabios.c:379
    xor ah, ah                                ; 30 e4                       ; 0xc0b20
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0b22
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0b25 vgabios.c:380
    jmp short 00ae7h                          ; eb be                       ; 0xc0b27 vgabios.c:381
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0b29 vgabios.c:383
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b2c
    pop di                                    ; 5f                          ; 0xc0b2f
    pop si                                    ; 5e                          ; 0xc0b30
    pop bp                                    ; 5d                          ; 0xc0b31
    retn 00004h                               ; c2 04 00                    ; 0xc0b32
  ; disGetNextSymbol 0xc0b35 LB 0x3ad8 -> off=0x0 cb=0000000000000046 uValue=00000000000c0b35 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0b35 LB 0x46
    push bp                                   ; 55                          ; 0xc0b35 vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b36
    push si                                   ; 56                          ; 0xc0b38
    push di                                   ; 57                          ; 0xc0b39
    push ax                                   ; 50                          ; 0xc0b3a
    push ax                                   ; 50                          ; 0xc0b3b
    mov si, ax                                ; 89 c6                       ; 0xc0b3c
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0b3e
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0b41
    mov bx, cx                                ; 89 cb                       ; 0xc0b44
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0b46 vgabios.c:392
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b49
    out DX, ax                                ; ef                          ; 0xc0b4c
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0b4d vgabios.c:394
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0b50
    je short 00b6bh                           ; 74 15                       ; 0xc0b54
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0b56 vgabios.c:395
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0b59
    not al                                    ; f6 d0                       ; 0xc0b5c
    mov di, bx                                ; 89 df                       ; 0xc0b5e
    inc bx                                    ; 43                          ; 0xc0b60
    push SS                                   ; 16                          ; 0xc0b61
    pop ES                                    ; 07                          ; 0xc0b62
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0b63
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0b66 vgabios.c:396
    jmp short 00b4dh                          ; eb e2                       ; 0xc0b69 vgabios.c:397
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0b6b vgabios.c:400
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b6e
    out DX, ax                                ; ef                          ; 0xc0b71
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b72 vgabios.c:401
    pop di                                    ; 5f                          ; 0xc0b75
    pop si                                    ; 5e                          ; 0xc0b76
    pop bp                                    ; 5d                          ; 0xc0b77
    retn 00002h                               ; c2 02 00                    ; 0xc0b78
  ; disGetNextSymbol 0xc0b7b LB 0x3a92 -> off=0x0 cb=000000000000002f uValue=00000000000c0b7b 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0b7b LB 0x2f
    push si                                   ; 56                          ; 0xc0b7b vgabios.c:403
    push bp                                   ; 55                          ; 0xc0b7c
    mov bp, sp                                ; 89 e5                       ; 0xc0b7d
    mov ch, al                                ; 88 c5                       ; 0xc0b7f
    mov al, dl                                ; 88 d0                       ; 0xc0b81
    xor ah, ah                                ; 30 e4                       ; 0xc0b83 vgabios.c:407
    mul bx                                    ; f7 e3                       ; 0xc0b85
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0b87
    xor bh, bh                                ; 30 ff                       ; 0xc0b8a
    mul bx                                    ; f7 e3                       ; 0xc0b8c
    mov bl, ch                                ; 88 eb                       ; 0xc0b8e
    add bx, ax                                ; 01 c3                       ; 0xc0b90
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b92 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b95
    mov es, ax                                ; 8e c0                       ; 0xc0b98
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b9a
    mov al, cl                                ; 88 c8                       ; 0xc0b9d vgabios.c:58
    xor ah, ah                                ; 30 e4                       ; 0xc0b9f
    mul si                                    ; f7 e6                       ; 0xc0ba1
    add ax, bx                                ; 01 d8                       ; 0xc0ba3
    pop bp                                    ; 5d                          ; 0xc0ba5 vgabios.c:411
    pop si                                    ; 5e                          ; 0xc0ba6
    retn 00002h                               ; c2 02 00                    ; 0xc0ba7
  ; disGetNextSymbol 0xc0baa LB 0x3a63 -> off=0x0 cb=0000000000000045 uValue=00000000000c0baa 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0baa LB 0x45
    push bp                                   ; 55                          ; 0xc0baa vgabios.c:413
    mov bp, sp                                ; 89 e5                       ; 0xc0bab
    push cx                                   ; 51                          ; 0xc0bad
    push si                                   ; 56                          ; 0xc0bae
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0baf
    mov si, ax                                ; 89 c6                       ; 0xc0bb2
    mov ax, dx                                ; 89 d0                       ; 0xc0bb4
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0bb6 vgabios.c:417
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0bb9
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0bbd
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0bc0
    mov bx, si                                ; 89 f3                       ; 0xc0bc3
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bc5
    call 00b35h                               ; e8 6a ff                    ; 0xc0bc8
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0bcb vgabios.c:420
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0bce
    push ax                                   ; 50                          ; 0xc0bd1
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0bd2 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0bd5
    mov es, ax                                ; 8e c0                       ; 0xc0bd7
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0bd9
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0bdc
    xor cx, cx                                ; 31 c9                       ; 0xc0be0 vgabios.c:68
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0be2
    call 00ad7h                               ; e8 ef fe                    ; 0xc0be5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0be8 vgabios.c:421
    pop si                                    ; 5e                          ; 0xc0beb
    pop cx                                    ; 59                          ; 0xc0bec
    pop bp                                    ; 5d                          ; 0xc0bed
    retn                                      ; c3                          ; 0xc0bee
  ; disGetNextSymbol 0xc0bef LB 0x3a1e -> off=0x0 cb=0000000000000027 uValue=00000000000c0bef 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0bef LB 0x27
    push bp                                   ; 55                          ; 0xc0bef vgabios.c:423
    mov bp, sp                                ; 89 e5                       ; 0xc0bf0
    push ax                                   ; 50                          ; 0xc0bf2
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0bf3
    mov al, dl                                ; 88 d0                       ; 0xc0bf6 vgabios.c:427
    xor ah, ah                                ; 30 e4                       ; 0xc0bf8
    mul bx                                    ; f7 e3                       ; 0xc0bfa
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0bfc
    xor dh, dh                                ; 30 f6                       ; 0xc0bff
    mul dx                                    ; f7 e2                       ; 0xc0c01
    mov dx, ax                                ; 89 c2                       ; 0xc0c03
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0c05
    xor ah, ah                                ; 30 e4                       ; 0xc0c08
    add ax, dx                                ; 01 d0                       ; 0xc0c0a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0c0c vgabios.c:428
    sal ax, CL                                ; d3 e0                       ; 0xc0c0e
    mov sp, bp                                ; 89 ec                       ; 0xc0c10 vgabios.c:430
    pop bp                                    ; 5d                          ; 0xc0c12
    retn 00002h                               ; c2 02 00                    ; 0xc0c13
  ; disGetNextSymbol 0xc0c16 LB 0x39f7 -> off=0x0 cb=000000000000004e uValue=00000000000c0c16 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0c16 LB 0x4e
    push si                                   ; 56                          ; 0xc0c16 vgabios.c:432
    push di                                   ; 57                          ; 0xc0c17
    push bp                                   ; 55                          ; 0xc0c18
    mov bp, sp                                ; 89 e5                       ; 0xc0c19
    push ax                                   ; 50                          ; 0xc0c1b
    push ax                                   ; 50                          ; 0xc0c1c
    mov si, ax                                ; 89 c6                       ; 0xc0c1d
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0c1f
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0c22
    mov bx, cx                                ; 89 cb                       ; 0xc0c25
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0c27 vgabios.c:438
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0c2a
    je short 00c5ch                           ; 74 2c                       ; 0xc0c2e
    xor dh, dh                                ; 30 f6                       ; 0xc0c30 vgabios.c:439
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0c32 vgabios.c:440
    xor ax, ax                                ; 31 c0                       ; 0xc0c34 vgabios.c:441
    jmp short 00c3dh                          ; eb 05                       ; 0xc0c36
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0c38
    jnl short 00c51h                          ; 7d 14                       ; 0xc0c3b
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0c3d vgabios.c:442
    mov di, si                                ; 89 f7                       ; 0xc0c40
    add di, ax                                ; 01 c7                       ; 0xc0c42
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0c44
    je short 00c4ch                           ; 74 02                       ; 0xc0c48
    or dh, dl                                 ; 08 d6                       ; 0xc0c4a vgabios.c:443
    shr dl, 1                                 ; d0 ea                       ; 0xc0c4c vgabios.c:444
    inc ax                                    ; 40                          ; 0xc0c4e vgabios.c:445
    jmp short 00c38h                          ; eb e7                       ; 0xc0c4f
    mov di, bx                                ; 89 df                       ; 0xc0c51 vgabios.c:446
    inc bx                                    ; 43                          ; 0xc0c53
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0c54
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0c57 vgabios.c:447
    jmp short 00c27h                          ; eb cb                       ; 0xc0c5a vgabios.c:448
    mov sp, bp                                ; 89 ec                       ; 0xc0c5c vgabios.c:449
    pop bp                                    ; 5d                          ; 0xc0c5e
    pop di                                    ; 5f                          ; 0xc0c5f
    pop si                                    ; 5e                          ; 0xc0c60
    retn 00002h                               ; c2 02 00                    ; 0xc0c61
  ; disGetNextSymbol 0xc0c64 LB 0x39a9 -> off=0x0 cb=0000000000000049 uValue=00000000000c0c64 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0c64 LB 0x49
    push bp                                   ; 55                          ; 0xc0c64 vgabios.c:451
    mov bp, sp                                ; 89 e5                       ; 0xc0c65
    push cx                                   ; 51                          ; 0xc0c67
    push si                                   ; 56                          ; 0xc0c68
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0c69
    mov si, ax                                ; 89 c6                       ; 0xc0c6c
    mov ax, dx                                ; 89 d0                       ; 0xc0c6e
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0c70 vgabios.c:455
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0c73
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0c77
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0c7a
    mov bx, si                                ; 89 f3                       ; 0xc0c7c
    sal bx, CL                                ; d3 e3                       ; 0xc0c7e
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0c80
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0c83
    call 00c16h                               ; e8 8d ff                    ; 0xc0c86
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0c89 vgabios.c:458
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0c8c
    push ax                                   ; 50                          ; 0xc0c8f
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c90 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0c93
    mov es, ax                                ; 8e c0                       ; 0xc0c95
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c97
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c9a
    xor cx, cx                                ; 31 c9                       ; 0xc0c9e vgabios.c:68
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0ca0
    call 00ad7h                               ; e8 31 fe                    ; 0xc0ca3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ca6 vgabios.c:459
    pop si                                    ; 5e                          ; 0xc0ca9
    pop cx                                    ; 59                          ; 0xc0caa
    pop bp                                    ; 5d                          ; 0xc0cab
    retn                                      ; c3                          ; 0xc0cac
  ; disGetNextSymbol 0xc0cad LB 0x3960 -> off=0x0 cb=0000000000000036 uValue=00000000000c0cad 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0cad LB 0x36
    push bp                                   ; 55                          ; 0xc0cad vgabios.c:461
    mov bp, sp                                ; 89 e5                       ; 0xc0cae
    push bx                                   ; 53                          ; 0xc0cb0
    push cx                                   ; 51                          ; 0xc0cb1
    mov bx, ax                                ; 89 c3                       ; 0xc0cb2
    mov es, dx                                ; 8e c2                       ; 0xc0cb4
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0cb6 vgabios.c:467
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0cb9 vgabios.c:468
    xor dl, dl                                ; 30 d2                       ; 0xc0cbb vgabios.c:469
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0cbd vgabios.c:470
    xchg ah, al                               ; 86 c4                       ; 0xc0cc0
    xor bx, bx                                ; 31 db                       ; 0xc0cc2 vgabios.c:472
    jmp short 00ccbh                          ; eb 05                       ; 0xc0cc4
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0cc6
    jnl short 00cdah                          ; 7d 0f                       ; 0xc0cc9
    test ax, cx                               ; 85 c8                       ; 0xc0ccb vgabios.c:473
    je short 00cd1h                           ; 74 02                       ; 0xc0ccd
    or dl, dh                                 ; 08 f2                       ; 0xc0ccf vgabios.c:474
    shr dh, 1                                 ; d0 ee                       ; 0xc0cd1 vgabios.c:475
    shr cx, 1                                 ; d1 e9                       ; 0xc0cd3 vgabios.c:476
    shr cx, 1                                 ; d1 e9                       ; 0xc0cd5
    inc bx                                    ; 43                          ; 0xc0cd7 vgabios.c:477
    jmp short 00cc6h                          ; eb ec                       ; 0xc0cd8
    mov al, dl                                ; 88 d0                       ; 0xc0cda vgabios.c:479
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0cdc
    pop cx                                    ; 59                          ; 0xc0cdf
    pop bx                                    ; 5b                          ; 0xc0ce0
    pop bp                                    ; 5d                          ; 0xc0ce1
    retn                                      ; c3                          ; 0xc0ce2
  ; disGetNextSymbol 0xc0ce3 LB 0x392a -> off=0x0 cb=0000000000000084 uValue=00000000000c0ce3 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0ce3 LB 0x84
    push bp                                   ; 55                          ; 0xc0ce3 vgabios.c:481
    mov bp, sp                                ; 89 e5                       ; 0xc0ce4
    push cx                                   ; 51                          ; 0xc0ce6
    push si                                   ; 56                          ; 0xc0ce7
    push di                                   ; 57                          ; 0xc0ce8
    push ax                                   ; 50                          ; 0xc0ce9
    mov si, dx                                ; 89 d6                       ; 0xc0cea
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0cec vgabios.c:489
    je short 00d2bh                           ; 74 3a                       ; 0xc0cef
    mov bx, ax                                ; 89 c3                       ; 0xc0cf1 vgabios.c:491
    sal bx, 1                                 ; d1 e3                       ; 0xc0cf3
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0cf5
    xor cx, cx                                ; 31 c9                       ; 0xc0cfa vgabios.c:493
    jmp short 00d03h                          ; eb 05                       ; 0xc0cfc
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0cfe
    jnl short 00d5fh                          ; 7d 5c                       ; 0xc0d01
    mov ax, bx                                ; 89 d8                       ; 0xc0d03 vgabios.c:494
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0d05
    call 00cadh                               ; e8 a2 ff                    ; 0xc0d08
    mov di, si                                ; 89 f7                       ; 0xc0d0b
    inc si                                    ; 46                          ; 0xc0d0d
    push SS                                   ; 16                          ; 0xc0d0e
    pop ES                                    ; 07                          ; 0xc0d0f
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d10
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0d13 vgabios.c:495
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0d17
    call 00cadh                               ; e8 90 ff                    ; 0xc0d1a
    mov di, si                                ; 89 f7                       ; 0xc0d1d
    inc si                                    ; 46                          ; 0xc0d1f
    push SS                                   ; 16                          ; 0xc0d20
    pop ES                                    ; 07                          ; 0xc0d21
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d22
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0d25 vgabios.c:496
    inc cx                                    ; 41                          ; 0xc0d28 vgabios.c:497
    jmp short 00cfeh                          ; eb d3                       ; 0xc0d29
    mov bx, ax                                ; 89 c3                       ; 0xc0d2b vgabios.c:499
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0d2d
    xor cx, cx                                ; 31 c9                       ; 0xc0d32 vgabios.c:500
    jmp short 00d3bh                          ; eb 05                       ; 0xc0d34
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0d36
    jnl short 00d5fh                          ; 7d 24                       ; 0xc0d39
    mov di, si                                ; 89 f7                       ; 0xc0d3b vgabios.c:501
    inc si                                    ; 46                          ; 0xc0d3d
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d3e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d41
    push SS                                   ; 16                          ; 0xc0d44
    pop ES                                    ; 07                          ; 0xc0d45
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d46
    mov di, si                                ; 89 f7                       ; 0xc0d49 vgabios.c:502
    inc si                                    ; 46                          ; 0xc0d4b
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d4c
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0d4f
    push SS                                   ; 16                          ; 0xc0d54
    pop ES                                    ; 07                          ; 0xc0d55
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d56
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0d59 vgabios.c:503
    inc cx                                    ; 41                          ; 0xc0d5c vgabios.c:504
    jmp short 00d36h                          ; eb d7                       ; 0xc0d5d
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0d5f vgabios.c:506
    pop di                                    ; 5f                          ; 0xc0d62
    pop si                                    ; 5e                          ; 0xc0d63
    pop cx                                    ; 59                          ; 0xc0d64
    pop bp                                    ; 5d                          ; 0xc0d65
    retn                                      ; c3                          ; 0xc0d66
  ; disGetNextSymbol 0xc0d67 LB 0x38a6 -> off=0x0 cb=000000000000001b uValue=00000000000c0d67 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0d67 LB 0x1b
    push cx                                   ; 51                          ; 0xc0d67 vgabios.c:508
    push bp                                   ; 55                          ; 0xc0d68
    mov bp, sp                                ; 89 e5                       ; 0xc0d69
    mov cl, al                                ; 88 c1                       ; 0xc0d6b
    mov al, dl                                ; 88 d0                       ; 0xc0d6d
    xor ah, ah                                ; 30 e4                       ; 0xc0d6f vgabios.c:513
    mul bx                                    ; f7 e3                       ; 0xc0d71
    mov bx, ax                                ; 89 c3                       ; 0xc0d73
    sal bx, 1                                 ; d1 e3                       ; 0xc0d75
    sal bx, 1                                 ; d1 e3                       ; 0xc0d77
    mov al, cl                                ; 88 c8                       ; 0xc0d79
    xor ah, ah                                ; 30 e4                       ; 0xc0d7b
    add ax, bx                                ; 01 d8                       ; 0xc0d7d
    pop bp                                    ; 5d                          ; 0xc0d7f vgabios.c:514
    pop cx                                    ; 59                          ; 0xc0d80
    retn                                      ; c3                          ; 0xc0d81
  ; disGetNextSymbol 0xc0d82 LB 0x388b -> off=0x0 cb=000000000000006b uValue=00000000000c0d82 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0d82 LB 0x6b
    push bp                                   ; 55                          ; 0xc0d82 vgabios.c:516
    mov bp, sp                                ; 89 e5                       ; 0xc0d83
    push bx                                   ; 53                          ; 0xc0d85
    push cx                                   ; 51                          ; 0xc0d86
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0d87
    mov bl, dl                                ; 88 d3                       ; 0xc0d8a vgabios.c:522
    xor bh, bh                                ; 30 ff                       ; 0xc0d8c
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d8e
    call 00ce3h                               ; e8 4f ff                    ; 0xc0d91
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0d94 vgabios.c:525
    push ax                                   ; 50                          ; 0xc0d97
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0d98
    push ax                                   ; 50                          ; 0xc0d9b
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d9c vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0d9f
    mov es, ax                                ; 8e c0                       ; 0xc0da1
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0da3
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0da6
    xor cx, cx                                ; 31 c9                       ; 0xc0daa vgabios.c:68
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0dac
    call 00ad7h                               ; e8 25 fd                    ; 0xc0daf
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0db2
    test ah, 080h                             ; f6 c4 80                    ; 0xc0db5 vgabios.c:527
    jne short 00de3h                          ; 75 29                       ; 0xc0db8
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0dba vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0dbd
    mov es, ax                                ; 8e c0                       ; 0xc0dbf
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0dc1
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0dc4
    test dx, dx                               ; 85 d2                       ; 0xc0dc8 vgabios.c:531
    jne short 00dd0h                          ; 75 04                       ; 0xc0dca
    test ax, ax                               ; 85 c0                       ; 0xc0dcc
    je short 00de3h                           ; 74 13                       ; 0xc0dce
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc0dd0 vgabios.c:532
    push bx                                   ; 53                          ; 0xc0dd3
    mov bx, 00080h                            ; bb 80 00                    ; 0xc0dd4
    push bx                                   ; 53                          ; 0xc0dd7
    mov cx, bx                                ; 89 d9                       ; 0xc0dd8
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0dda
    call 00ad7h                               ; e8 f7 fc                    ; 0xc0ddd
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0de0
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0de3 vgabios.c:535
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0de6
    pop cx                                    ; 59                          ; 0xc0de9
    pop bx                                    ; 5b                          ; 0xc0dea
    pop bp                                    ; 5d                          ; 0xc0deb
    retn                                      ; c3                          ; 0xc0dec
  ; disGetNextSymbol 0xc0ded LB 0x3820 -> off=0x0 cb=0000000000000147 uValue=00000000000c0ded 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0ded LB 0x147
    push bp                                   ; 55                          ; 0xc0ded vgabios.c:537
    mov bp, sp                                ; 89 e5                       ; 0xc0dee
    push bx                                   ; 53                          ; 0xc0df0
    push cx                                   ; 51                          ; 0xc0df1
    push si                                   ; 56                          ; 0xc0df2
    push di                                   ; 57                          ; 0xc0df3
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0df4
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0df7
    mov si, dx                                ; 89 d6                       ; 0xc0dfa
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0dfc vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0dff
    mov es, ax                                ; 8e c0                       ; 0xc0e02
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0e04
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0e07 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0e0a vgabios.c:545
    call 03940h                               ; e8 31 2b                    ; 0xc0e0c
    mov cl, al                                ; 88 c1                       ; 0xc0e0f
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0e11 vgabios.c:546
    jne short 00e18h                          ; 75 03                       ; 0xc0e13
    jmp near 00f2bh                           ; e9 13 01                    ; 0xc0e15
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc0e18 vgabios.c:550
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc0e1b
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc0e1e
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc0e22
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc0e25
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc0e28
    call 00a97h                               ; e8 69 fc                    ; 0xc0e2b
    mov ch, byte [bp-01ah]                    ; 8a 6e e6                    ; 0xc0e2e vgabios.c:551
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc0e31 vgabios.c:552
    mov al, ah                                ; 88 e0                       ; 0xc0e34
    xor ah, ah                                ; 30 e4                       ; 0xc0e36
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc0e38
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0e3b
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0e3e
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0e41 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e44
    mov es, ax                                ; 8e c0                       ; 0xc0e47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0e49
    xor ah, ah                                ; 30 e4                       ; 0xc0e4c vgabios.c:48
    mov dx, ax                                ; 89 c2                       ; 0xc0e4e
    inc dx                                    ; 42                          ; 0xc0e50
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0e51 vgabios.c:57
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0e54
    mov word [bp-016h], di                    ; 89 7e ea                    ; 0xc0e57 vgabios.c:58
    mov bl, cl                                ; 88 cb                       ; 0xc0e5a vgabios.c:558
    xor bh, bh                                ; 30 ff                       ; 0xc0e5c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0e5e
    sal bx, CL                                ; d3 e3                       ; 0xc0e60
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0e62
    jne short 00e9fh                          ; 75 36                       ; 0xc0e67
    mov ax, di                                ; 89 f8                       ; 0xc0e69 vgabios.c:560
    mul dx                                    ; f7 e2                       ; 0xc0e6b
    sal ax, 1                                 ; d1 e0                       ; 0xc0e6d
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0e6f
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc0e71
    xor dh, dh                                ; 30 f6                       ; 0xc0e74
    inc ax                                    ; 40                          ; 0xc0e76
    mul dx                                    ; f7 e2                       ; 0xc0e77
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc0e79
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0e7c
    xor ah, ah                                ; 30 e4                       ; 0xc0e7f
    mul di                                    ; f7 e7                       ; 0xc0e81
    mov dl, ch                                ; 88 ea                       ; 0xc0e83
    xor dh, dh                                ; 30 f6                       ; 0xc0e85
    add ax, dx                                ; 01 d0                       ; 0xc0e87
    sal ax, 1                                 ; d1 e0                       ; 0xc0e89
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc0e8b
    add di, ax                                ; 01 c7                       ; 0xc0e8e
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0e90 vgabios.c:55
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0e94
    push SS                                   ; 16                          ; 0xc0e97 vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0e98
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e99
    jmp near 00f2bh                           ; e9 8c 00                    ; 0xc0e9c vgabios.c:562
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0e9f vgabios.c:563
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0ea3
    je short 00efeh                           ; 74 56                       ; 0xc0ea6
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0ea8
    jc short 00eb4h                           ; 72 07                       ; 0xc0eab
    jbe short 00eb6h                          ; 76 07                       ; 0xc0ead
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0eaf
    jbe short 00ed1h                          ; 76 1d                       ; 0xc0eb2
    jmp short 00f2bh                          ; eb 75                       ; 0xc0eb4
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc0eb6 vgabios.c:566
    xor dh, dh                                ; 30 f6                       ; 0xc0eb9
    mov al, ch                                ; 88 e8                       ; 0xc0ebb
    xor ah, ah                                ; 30 e4                       ; 0xc0ebd
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0ebf
    call 00d67h                               ; e8 a2 fe                    ; 0xc0ec2
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0ec5 vgabios.c:567
    xor dh, dh                                ; 30 f6                       ; 0xc0ec8
    call 00d82h                               ; e8 b5 fe                    ; 0xc0eca
    xor ah, ah                                ; 30 e4                       ; 0xc0ecd
    jmp short 00e97h                          ; eb c6                       ; 0xc0ecf
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0ed1 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0ed4
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0ed7 vgabios.c:572
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0eda
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0ede
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0ee1
    xor dh, dh                                ; 30 f6                       ; 0xc0ee4
    mov al, ch                                ; 88 e8                       ; 0xc0ee6
    xor ah, ah                                ; 30 e4                       ; 0xc0ee8
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0eea
    mov bx, di                                ; 89 fb                       ; 0xc0eed
    call 00b7bh                               ; e8 89 fc                    ; 0xc0eef
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0ef2 vgabios.c:573
    mov dx, ax                                ; 89 c2                       ; 0xc0ef5
    mov ax, di                                ; 89 f8                       ; 0xc0ef7
    call 00baah                               ; e8 ae fc                    ; 0xc0ef9
    jmp short 00ecdh                          ; eb cf                       ; 0xc0efc
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0efe vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0f01
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0f04 vgabios.c:577
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0f07
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0f0b
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0f0e
    xor dh, dh                                ; 30 f6                       ; 0xc0f11
    mov al, ch                                ; 88 e8                       ; 0xc0f13
    xor ah, ah                                ; 30 e4                       ; 0xc0f15
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0f17
    mov bx, di                                ; 89 fb                       ; 0xc0f1a
    call 00befh                               ; e8 d0 fc                    ; 0xc0f1c
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0f1f vgabios.c:578
    mov dx, ax                                ; 89 c2                       ; 0xc0f22
    mov ax, di                                ; 89 f8                       ; 0xc0f24
    call 00c64h                               ; e8 3b fd                    ; 0xc0f26
    jmp short 00ecdh                          ; eb a2                       ; 0xc0f29
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0f2b vgabios.c:587
    pop di                                    ; 5f                          ; 0xc0f2e
    pop si                                    ; 5e                          ; 0xc0f2f
    pop cx                                    ; 59                          ; 0xc0f30
    pop bx                                    ; 5b                          ; 0xc0f31
    pop bp                                    ; 5d                          ; 0xc0f32
    retn                                      ; c3                          ; 0xc0f33
  ; disGetNextSymbol 0xc0f34 LB 0x36d9 -> off=0x10 cb=0000000000000083 uValue=00000000000c0f44 'vga_get_font_info'
    db  05bh, 00fh, 0a0h, 00fh, 0a5h, 00fh, 0ach, 00fh, 0b1h, 00fh, 0b6h, 00fh, 0bbh, 00fh, 0c0h, 00fh
vga_get_font_info:                           ; 0xc0f44 LB 0x83
    push si                                   ; 56                          ; 0xc0f44 vgabios.c:589
    push di                                   ; 57                          ; 0xc0f45
    push bp                                   ; 55                          ; 0xc0f46
    mov bp, sp                                ; 89 e5                       ; 0xc0f47
    mov si, dx                                ; 89 d6                       ; 0xc0f49
    mov di, bx                                ; 89 df                       ; 0xc0f4b
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0f4d vgabios.c:594
    jnbe short 00f9ah                         ; 77 48                       ; 0xc0f50
    mov bx, ax                                ; 89 c3                       ; 0xc0f52
    sal bx, 1                                 ; d1 e3                       ; 0xc0f54
    jmp word [cs:bx+00f34h]                   ; 2e ff a7 34 0f              ; 0xc0f56
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0f5b vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0f5e
    mov es, ax                                ; 8e c0                       ; 0xc0f60
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0f62
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0f65
    push SS                                   ; 16                          ; 0xc0f69 vgabios.c:597
    pop ES                                    ; 07                          ; 0xc0f6a
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0f6b
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0f6e
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0f71
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f74
    mov es, ax                                ; 8e c0                       ; 0xc0f77
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f79
    xor ah, ah                                ; 30 e4                       ; 0xc0f7c
    push SS                                   ; 16                          ; 0xc0f7e
    pop ES                                    ; 07                          ; 0xc0f7f
    mov bx, cx                                ; 89 cb                       ; 0xc0f80
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f82
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0f85
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f88
    mov es, ax                                ; 8e c0                       ; 0xc0f8b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f8d
    xor ah, ah                                ; 30 e4                       ; 0xc0f90
    push SS                                   ; 16                          ; 0xc0f92
    pop ES                                    ; 07                          ; 0xc0f93
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0f94
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f97
    pop bp                                    ; 5d                          ; 0xc0f9a
    pop di                                    ; 5f                          ; 0xc0f9b
    pop si                                    ; 5e                          ; 0xc0f9c
    retn 00002h                               ; c2 02 00                    ; 0xc0f9d
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0fa0 vgabios.c:67
    jmp short 00f5eh                          ; eb b9                       ; 0xc0fa3
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc0fa5 vgabios.c:602
    mov ax, ds                                ; 8c d8                       ; 0xc0fa8
    jmp short 00f69h                          ; eb bd                       ; 0xc0faa vgabios.c:603
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc0fac vgabios.c:605
    jmp short 00fa8h                          ; eb f7                       ; 0xc0faf
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc0fb1 vgabios.c:608
    jmp short 00fa8h                          ; eb f2                       ; 0xc0fb4
    mov dx, 07b6ch                            ; ba 6c 7b                    ; 0xc0fb6 vgabios.c:611
    jmp short 00fa8h                          ; eb ed                       ; 0xc0fb9
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc0fbb vgabios.c:614
    jmp short 00fa8h                          ; eb e8                       ; 0xc0fbe
    mov dx, 07c99h                            ; ba 99 7c                    ; 0xc0fc0 vgabios.c:617
    jmp short 00fa8h                          ; eb e3                       ; 0xc0fc3
    jmp short 00f9ah                          ; eb d3                       ; 0xc0fc5 vgabios.c:623
  ; disGetNextSymbol 0xc0fc7 LB 0x3646 -> off=0x0 cb=000000000000016d uValue=00000000000c0fc7 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0fc7 LB 0x16d
    push bp                                   ; 55                          ; 0xc0fc7 vgabios.c:636
    mov bp, sp                                ; 89 e5                       ; 0xc0fc8
    push si                                   ; 56                          ; 0xc0fca
    push di                                   ; 57                          ; 0xc0fcb
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc0fcc
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0fcf
    mov si, dx                                ; 89 d6                       ; 0xc0fd2
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc0fd4
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc0fd7
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0fda vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fdd
    mov es, ax                                ; 8e c0                       ; 0xc0fe0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fe2
    xor ah, ah                                ; 30 e4                       ; 0xc0fe5 vgabios.c:643
    call 03940h                               ; e8 56 29                    ; 0xc0fe7
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc0fea
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0fed vgabios.c:644
    je short 01000h                           ; 74 0f                       ; 0xc0fef
    mov bl, al                                ; 88 c3                       ; 0xc0ff1 vgabios.c:646
    xor bh, bh                                ; 30 ff                       ; 0xc0ff3
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0ff5
    sal bx, CL                                ; d3 e3                       ; 0xc0ff7
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0ff9
    jne short 01003h                          ; 75 03                       ; 0xc0ffe
    jmp near 0112dh                           ; e9 2a 01                    ; 0xc1000 vgabios.c:647
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc1003 vgabios.c:650
    cmp ch, cl                                ; 38 cd                       ; 0xc1007
    jc short 0101ah                           ; 72 0f                       ; 0xc1009
    jbe short 01022h                          ; 76 15                       ; 0xc100b
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc100d
    je short 0105bh                           ; 74 49                       ; 0xc1010
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc1012
    je short 01022h                           ; 74 0b                       ; 0xc1015
    jmp near 01123h                           ; e9 09 01                    ; 0xc1017
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc101a
    je short 0108fh                           ; 74 70                       ; 0xc101d
    jmp near 01123h                           ; e9 01 01                    ; 0xc101f
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1022 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1025
    mov es, ax                                ; 8e c0                       ; 0xc1028
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc102a
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc102d vgabios.c:58
    mul bx                                    ; f7 e3                       ; 0xc1030
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1032
    mov bx, si                                ; 89 f3                       ; 0xc1034
    shr bx, CL                                ; d3 eb                       ; 0xc1036
    add bx, ax                                ; 01 c3                       ; 0xc1038
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc103a vgabios.c:57
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc103d
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc1040 vgabios.c:58
    xor ch, ch                                ; 30 ed                       ; 0xc1043
    mul cx                                    ; f7 e1                       ; 0xc1045
    add bx, ax                                ; 01 c3                       ; 0xc1047
    mov cx, si                                ; 89 f1                       ; 0xc1049 vgabios.c:655
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc104b
    mov ax, 00080h                            ; b8 80 00                    ; 0xc104e
    sar ax, CL                                ; d3 f8                       ; 0xc1051
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1053
    mov byte [bp-008h], ch                    ; 88 6e f8                    ; 0xc1056 vgabios.c:657
    jmp short 01064h                          ; eb 09                       ; 0xc1059
    jmp near 01103h                           ; e9 a5 00                    ; 0xc105b
    cmp byte [bp-008h], 004h                  ; 80 7e f8 04                 ; 0xc105e
    jnc short 0108ch                          ; 73 28                       ; 0xc1062
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc1064 vgabios.c:658
    xor al, al                                ; 30 c0                       ; 0xc1067
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc1069
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc106b
    out DX, ax                                ; ef                          ; 0xc106e
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc106f vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc1072
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1074
    and al, byte [bp-00ah]                    ; 22 46 f6                    ; 0xc1077 vgabios.c:48
    test al, al                               ; 84 c0                       ; 0xc107a vgabios.c:660
    jbe short 01087h                          ; 76 09                       ; 0xc107c
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc107e vgabios.c:661
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc1081
    sal al, CL                                ; d2 e0                       ; 0xc1083
    or ch, al                                 ; 08 c5                       ; 0xc1085
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1087 vgabios.c:662
    jmp short 0105eh                          ; eb d2                       ; 0xc108a
    jmp near 01125h                           ; e9 96 00                    ; 0xc108c
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc108f vgabios.c:665
    xor ah, ah                                ; 30 e4                       ; 0xc1093
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc1095
    sub cx, ax                                ; 29 c1                       ; 0xc1098
    mov ax, dx                                ; 89 d0                       ; 0xc109a
    shr ax, CL                                ; d3 e8                       ; 0xc109c
    mov cx, ax                                ; 89 c1                       ; 0xc109e
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc10a0
    shr ax, 1                                 ; d1 e8                       ; 0xc10a3
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc10a5
    mul bx                                    ; f7 e3                       ; 0xc10a8
    mov bx, cx                                ; 89 cb                       ; 0xc10aa
    add bx, ax                                ; 01 c3                       ; 0xc10ac
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc10ae vgabios.c:666
    je short 010b7h                           ; 74 03                       ; 0xc10b2
    add bh, 020h                              ; 80 c7 20                    ; 0xc10b4 vgabios.c:667
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc10b7 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc10ba
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc10bc
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc10bf vgabios.c:669
    xor bh, bh                                ; 30 ff                       ; 0xc10c2
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc10c4
    sal bx, CL                                ; d3 e3                       ; 0xc10c6
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc10c8
    jne short 010eah                          ; 75 1b                       ; 0xc10cd
    mov cx, si                                ; 89 f1                       ; 0xc10cf vgabios.c:670
    xor ch, ch                                ; 30 ed                       ; 0xc10d1
    and cl, 003h                              ; 80 e1 03                    ; 0xc10d3
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc10d6
    sub bx, cx                                ; 29 cb                       ; 0xc10d9
    mov cx, bx                                ; 89 d9                       ; 0xc10db
    sal cx, 1                                 ; d1 e1                       ; 0xc10dd
    xor ah, ah                                ; 30 e4                       ; 0xc10df
    sar ax, CL                                ; d3 f8                       ; 0xc10e1
    mov ch, al                                ; 88 c5                       ; 0xc10e3
    and ch, 003h                              ; 80 e5 03                    ; 0xc10e5
    jmp short 01125h                          ; eb 3b                       ; 0xc10e8 vgabios.c:671
    mov cx, si                                ; 89 f1                       ; 0xc10ea vgabios.c:672
    xor ch, ch                                ; 30 ed                       ; 0xc10ec
    and cl, 007h                              ; 80 e1 07                    ; 0xc10ee
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc10f1
    sub bx, cx                                ; 29 cb                       ; 0xc10f4
    mov cx, bx                                ; 89 d9                       ; 0xc10f6
    xor ah, ah                                ; 30 e4                       ; 0xc10f8
    sar ax, CL                                ; d3 f8                       ; 0xc10fa
    mov ch, al                                ; 88 c5                       ; 0xc10fc
    and ch, 001h                              ; 80 e5 01                    ; 0xc10fe
    jmp short 01125h                          ; eb 22                       ; 0xc1101 vgabios.c:673
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1103 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1106
    mov es, ax                                ; 8e c0                       ; 0xc1109
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc110b
    sal bx, CL                                ; d3 e3                       ; 0xc110e vgabios.c:58
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc1110
    mul bx                                    ; f7 e3                       ; 0xc1113
    mov bx, si                                ; 89 f3                       ; 0xc1115
    add bx, ax                                ; 01 c3                       ; 0xc1117
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1119 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc111c
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc111e
    jmp short 01125h                          ; eb 02                       ; 0xc1121 vgabios.c:677
    xor ch, ch                                ; 30 ed                       ; 0xc1123 vgabios.c:682
    push SS                                   ; 16                          ; 0xc1125 vgabios.c:684
    pop ES                                    ; 07                          ; 0xc1126
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc1127
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc112a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc112d vgabios.c:685
    pop di                                    ; 5f                          ; 0xc1130
    pop si                                    ; 5e                          ; 0xc1131
    pop bp                                    ; 5d                          ; 0xc1132
    retn                                      ; c3                          ; 0xc1133
  ; disGetNextSymbol 0xc1134 LB 0x34d9 -> off=0x0 cb=000000000000009f uValue=00000000000c1134 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc1134 LB 0x9f
    push bp                                   ; 55                          ; 0xc1134 vgabios.c:690
    mov bp, sp                                ; 89 e5                       ; 0xc1135
    push bx                                   ; 53                          ; 0xc1137
    push cx                                   ; 51                          ; 0xc1138
    push si                                   ; 56                          ; 0xc1139
    push di                                   ; 57                          ; 0xc113a
    push ax                                   ; 50                          ; 0xc113b
    push ax                                   ; 50                          ; 0xc113c
    mov bx, ax                                ; 89 c3                       ; 0xc113d
    mov di, dx                                ; 89 d7                       ; 0xc113f
    mov dx, 003dah                            ; ba da 03                    ; 0xc1141 vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc1144
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1145
    xor al, al                                ; 30 c0                       ; 0xc1147 vgabios.c:696
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1149
    out DX, AL                                ; ee                          ; 0xc114c
    xor si, si                                ; 31 f6                       ; 0xc114d vgabios.c:698
    cmp si, di                                ; 39 fe                       ; 0xc114f
    jnc short 011b8h                          ; 73 65                       ; 0xc1151
    mov al, bl                                ; 88 d8                       ; 0xc1153 vgabios.c:701
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc1155
    out DX, AL                                ; ee                          ; 0xc1158
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1159 vgabios.c:703
    in AL, DX                                 ; ec                          ; 0xc115c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc115d
    mov cx, ax                                ; 89 c1                       ; 0xc115f
    in AL, DX                                 ; ec                          ; 0xc1161 vgabios.c:704
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1162
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1164
    in AL, DX                                 ; ec                          ; 0xc1167 vgabios.c:705
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1168
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc116a
    mov al, cl                                ; 88 c8                       ; 0xc116d vgabios.c:708
    xor ah, ah                                ; 30 e4                       ; 0xc116f
    mov cx, strict word 0004dh                ; b9 4d 00                    ; 0xc1171
    imul cx                                   ; f7 e9                       ; 0xc1174
    mov cx, ax                                ; 89 c1                       ; 0xc1176
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1178
    xor ah, ah                                ; 30 e4                       ; 0xc117b
    mov dx, 00097h                            ; ba 97 00                    ; 0xc117d
    imul dx                                   ; f7 ea                       ; 0xc1180
    add cx, ax                                ; 01 c1                       ; 0xc1182
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc1184
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc1187
    xor ch, ch                                ; 30 ed                       ; 0xc118a
    mov ax, cx                                ; 89 c8                       ; 0xc118c
    mov dx, strict word 0001ch                ; ba 1c 00                    ; 0xc118e
    imul dx                                   ; f7 ea                       ; 0xc1191
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc1193
    add ax, 00080h                            ; 05 80 00                    ; 0xc1196
    mov al, ah                                ; 88 e0                       ; 0xc1199
    cbw                                       ; 98                          ; 0xc119b
    mov cx, ax                                ; 89 c1                       ; 0xc119c
    cmp ax, strict word 0003fh                ; 3d 3f 00                    ; 0xc119e vgabios.c:710
    jbe short 011a6h                          ; 76 03                       ; 0xc11a1
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc11a3
    mov al, bl                                ; 88 d8                       ; 0xc11a6 vgabios.c:713
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc11a8
    out DX, AL                                ; ee                          ; 0xc11ab
    mov al, cl                                ; 88 c8                       ; 0xc11ac vgabios.c:715
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc11ae
    out DX, AL                                ; ee                          ; 0xc11b1
    out DX, AL                                ; ee                          ; 0xc11b2 vgabios.c:716
    out DX, AL                                ; ee                          ; 0xc11b3 vgabios.c:717
    inc bx                                    ; 43                          ; 0xc11b4 vgabios.c:718
    inc si                                    ; 46                          ; 0xc11b5 vgabios.c:719
    jmp short 0114fh                          ; eb 97                       ; 0xc11b6
    mov dx, 003dah                            ; ba da 03                    ; 0xc11b8 vgabios.c:720
    in AL, DX                                 ; ec                          ; 0xc11bb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc11bc
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc11be vgabios.c:721
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc11c0
    out DX, AL                                ; ee                          ; 0xc11c3
    mov dx, 003dah                            ; ba da 03                    ; 0xc11c4 vgabios.c:723
    in AL, DX                                 ; ec                          ; 0xc11c7
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc11c8
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc11ca vgabios.c:725
    pop di                                    ; 5f                          ; 0xc11cd
    pop si                                    ; 5e                          ; 0xc11ce
    pop cx                                    ; 59                          ; 0xc11cf
    pop bx                                    ; 5b                          ; 0xc11d0
    pop bp                                    ; 5d                          ; 0xc11d1
    retn                                      ; c3                          ; 0xc11d2
  ; disGetNextSymbol 0xc11d3 LB 0x343a -> off=0x0 cb=00000000000000fc uValue=00000000000c11d3 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc11d3 LB 0xfc
    push bp                                   ; 55                          ; 0xc11d3 vgabios.c:728
    mov bp, sp                                ; 89 e5                       ; 0xc11d4
    push bx                                   ; 53                          ; 0xc11d6
    push cx                                   ; 51                          ; 0xc11d7
    push si                                   ; 56                          ; 0xc11d8
    push ax                                   ; 50                          ; 0xc11d9
    push ax                                   ; 50                          ; 0xc11da
    mov ah, al                                ; 88 c4                       ; 0xc11db
    mov bl, dl                                ; 88 d3                       ; 0xc11dd
    mov dh, al                                ; 88 c6                       ; 0xc11df vgabios.c:734
    mov si, strict word 00060h                ; be 60 00                    ; 0xc11e1 vgabios.c:62
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc11e4
    mov es, cx                                ; 8e c1                       ; 0xc11e7
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc11e9
    mov si, 00087h                            ; be 87 00                    ; 0xc11ec vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc11ef
    test dl, 008h                             ; f6 c2 08                    ; 0xc11f2 vgabios.c:48
    jne short 01234h                          ; 75 3d                       ; 0xc11f5
    mov dl, al                                ; 88 c2                       ; 0xc11f7 vgabios.c:740
    and dl, 060h                              ; 80 e2 60                    ; 0xc11f9
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc11fc
    jne short 01207h                          ; 75 06                       ; 0xc11ff
    mov AH, strict byte 01eh                  ; b4 1e                       ; 0xc1201 vgabios.c:742
    xor bl, bl                                ; 30 db                       ; 0xc1203 vgabios.c:743
    jmp short 01234h                          ; eb 2d                       ; 0xc1205 vgabios.c:744
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1207 vgabios.c:47
    test dl, 001h                             ; f6 c2 01                    ; 0xc120a vgabios.c:48
    jne short 01269h                          ; 75 5a                       ; 0xc120d
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc120f
    jnc short 01269h                          ; 73 55                       ; 0xc1212
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc1214
    jnc short 01269h                          ; 73 50                       ; 0xc1217
    mov si, 00085h                            ; be 85 00                    ; 0xc1219 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc121c
    mov es, dx                                ; 8e c2                       ; 0xc121f
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1221
    mov dx, cx                                ; 89 ca                       ; 0xc1224 vgabios.c:58
    cmp bl, ah                                ; 38 e3                       ; 0xc1226 vgabios.c:755
    jnc short 01236h                          ; 73 0c                       ; 0xc1228
    test bl, bl                               ; 84 db                       ; 0xc122a vgabios.c:757
    je short 01269h                           ; 74 3b                       ; 0xc122c
    xor ah, ah                                ; 30 e4                       ; 0xc122e vgabios.c:758
    mov bl, cl                                ; 88 cb                       ; 0xc1230 vgabios.c:759
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1232
    jmp short 01269h                          ; eb 33                       ; 0xc1234 vgabios.c:761
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1236 vgabios.c:762
    xor al, al                                ; 30 c0                       ; 0xc1239
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc123b
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc123e
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1241
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1244
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc1247
    cmp si, cx                                ; 39 ce                       ; 0xc124a
    jnc short 0126bh                          ; 73 1d                       ; 0xc124c
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc124e
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc1251
    mov si, cx                                ; 89 ce                       ; 0xc1254
    dec si                                    ; 4e                          ; 0xc1256
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc1257
    je short 012a5h                           ; 74 49                       ; 0xc125a
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc125c
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc125f
    dec cx                                    ; 49                          ; 0xc1262
    dec cx                                    ; 49                          ; 0xc1263
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc1264
    jne short 0126bh                          ; 75 02                       ; 0xc1267
    jmp short 012a5h                          ; eb 3a                       ; 0xc1269
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc126b vgabios.c:764
    jbe short 012a5h                          ; 76 35                       ; 0xc126e
    mov cl, ah                                ; 88 e1                       ; 0xc1270 vgabios.c:765
    xor ch, ch                                ; 30 ed                       ; 0xc1272
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1274
    mov byte [bp-007h], ch                    ; 88 6e f9                    ; 0xc1277
    mov si, cx                                ; 89 ce                       ; 0xc127a
    inc si                                    ; 46                          ; 0xc127c
    inc si                                    ; 46                          ; 0xc127d
    mov cl, dl                                ; 88 d1                       ; 0xc127e
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc1280
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc1282
    jl short 0129ah                           ; 7c 13                       ; 0xc1285
    sub ah, bl                                ; 28 dc                       ; 0xc1287 vgabios.c:767
    add ah, dl                                ; 00 d4                       ; 0xc1289
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc128b
    mov bl, cl                                ; 88 cb                       ; 0xc128d vgabios.c:768
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc128f vgabios.c:769
    jc short 012a5h                           ; 72 11                       ; 0xc1292
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1294 vgabios.c:771
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1296 vgabios.c:772
    jmp short 012a5h                          ; eb 0b                       ; 0xc1298 vgabios.c:774
    cmp ah, 002h                              ; 80 fc 02                    ; 0xc129a
    jbe short 012a3h                          ; 76 04                       ; 0xc129d
    shr dx, 1                                 ; d1 ea                       ; 0xc129f vgabios.c:776
    mov ah, dl                                ; 88 d4                       ; 0xc12a1
    mov bl, cl                                ; 88 cb                       ; 0xc12a3 vgabios.c:780
    mov si, strict word 00063h                ; be 63 00                    ; 0xc12a5 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc12a8
    mov es, dx                                ; 8e c2                       ; 0xc12ab
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc12ad
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc12b0 vgabios.c:791
    mov dx, cx                                ; 89 ca                       ; 0xc12b2
    out DX, AL                                ; ee                          ; 0xc12b4
    mov si, cx                                ; 89 ce                       ; 0xc12b5 vgabios.c:792
    inc si                                    ; 46                          ; 0xc12b7
    mov al, ah                                ; 88 e0                       ; 0xc12b8
    mov dx, si                                ; 89 f2                       ; 0xc12ba
    out DX, AL                                ; ee                          ; 0xc12bc
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc12bd vgabios.c:793
    mov dx, cx                                ; 89 ca                       ; 0xc12bf
    out DX, AL                                ; ee                          ; 0xc12c1
    mov al, bl                                ; 88 d8                       ; 0xc12c2 vgabios.c:794
    mov dx, si                                ; 89 f2                       ; 0xc12c4
    out DX, AL                                ; ee                          ; 0xc12c6
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc12c7 vgabios.c:795
    pop si                                    ; 5e                          ; 0xc12ca
    pop cx                                    ; 59                          ; 0xc12cb
    pop bx                                    ; 5b                          ; 0xc12cc
    pop bp                                    ; 5d                          ; 0xc12cd
    retn                                      ; c3                          ; 0xc12ce
  ; disGetNextSymbol 0xc12cf LB 0x333e -> off=0x0 cb=000000000000008d uValue=00000000000c12cf 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc12cf LB 0x8d
    push bp                                   ; 55                          ; 0xc12cf vgabios.c:798
    mov bp, sp                                ; 89 e5                       ; 0xc12d0
    push bx                                   ; 53                          ; 0xc12d2
    push cx                                   ; 51                          ; 0xc12d3
    push si                                   ; 56                          ; 0xc12d4
    push di                                   ; 57                          ; 0xc12d5
    push ax                                   ; 50                          ; 0xc12d6
    mov bl, al                                ; 88 c3                       ; 0xc12d7
    mov cx, dx                                ; 89 d1                       ; 0xc12d9
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12db vgabios.c:804
    jnbe short 01353h                         ; 77 74                       ; 0xc12dd
    xor ah, ah                                ; 30 e4                       ; 0xc12df vgabios.c:807
    mov si, ax                                ; 89 c6                       ; 0xc12e1
    sal si, 1                                 ; d1 e6                       ; 0xc12e3
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc12e5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12e8 vgabios.c:62
    mov es, ax                                ; 8e c0                       ; 0xc12eb
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc12ed
    mov si, strict word 00062h                ; be 62 00                    ; 0xc12f0 vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc12f3
    cmp bl, al                                ; 38 c3                       ; 0xc12f6 vgabios.c:811
    jne short 01353h                          ; 75 59                       ; 0xc12f8
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc12fa vgabios.c:57
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc12fd
    mov di, 00084h                            ; bf 84 00                    ; 0xc1300 vgabios.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc1303
    xor ah, ah                                ; 30 e4                       ; 0xc1306 vgabios.c:48
    mov di, ax                                ; 89 c7                       ; 0xc1308
    inc di                                    ; 47                          ; 0xc130a
    mov ax, dx                                ; 89 d0                       ; 0xc130b vgabios.c:817
    mov al, dh                                ; 88 f0                       ; 0xc130d
    xor ah, dh                                ; 30 f4                       ; 0xc130f
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1311
    mov ax, si                                ; 89 f0                       ; 0xc1314 vgabios.c:820
    mul di                                    ; f7 e7                       ; 0xc1316
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1318
    xor bh, bh                                ; 30 ff                       ; 0xc131a
    inc ax                                    ; 40                          ; 0xc131c
    mul bx                                    ; f7 e3                       ; 0xc131d
    mov bx, ax                                ; 89 c3                       ; 0xc131f
    mov al, cl                                ; 88 c8                       ; 0xc1321
    xor ah, ah                                ; 30 e4                       ; 0xc1323
    add bx, ax                                ; 01 c3                       ; 0xc1325
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1327
    mul si                                    ; f7 e6                       ; 0xc132a
    mov si, bx                                ; 89 de                       ; 0xc132c
    add si, ax                                ; 01 c6                       ; 0xc132e
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1330 vgabios.c:57
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1333
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc1336 vgabios.c:824
    mov dx, bx                                ; 89 da                       ; 0xc1338
    out DX, AL                                ; ee                          ; 0xc133a
    mov ax, si                                ; 89 f0                       ; 0xc133b vgabios.c:825
    mov al, ah                                ; 88 e0                       ; 0xc133d
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc133f
    mov dx, cx                                ; 89 ca                       ; 0xc1342
    out DX, AL                                ; ee                          ; 0xc1344
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1345 vgabios.c:826
    mov dx, bx                                ; 89 da                       ; 0xc1347
    out DX, AL                                ; ee                          ; 0xc1349
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc134a vgabios.c:827
    mov ax, si                                ; 89 f0                       ; 0xc134e
    mov dx, cx                                ; 89 ca                       ; 0xc1350
    out DX, AL                                ; ee                          ; 0xc1352
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1353 vgabios.c:829
    pop di                                    ; 5f                          ; 0xc1356
    pop si                                    ; 5e                          ; 0xc1357
    pop cx                                    ; 59                          ; 0xc1358
    pop bx                                    ; 5b                          ; 0xc1359
    pop bp                                    ; 5d                          ; 0xc135a
    retn                                      ; c3                          ; 0xc135b
  ; disGetNextSymbol 0xc135c LB 0x32b1 -> off=0x0 cb=00000000000000d5 uValue=00000000000c135c 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc135c LB 0xd5
    push bp                                   ; 55                          ; 0xc135c vgabios.c:832
    mov bp, sp                                ; 89 e5                       ; 0xc135d
    push bx                                   ; 53                          ; 0xc135f
    push cx                                   ; 51                          ; 0xc1360
    push dx                                   ; 52                          ; 0xc1361
    push si                                   ; 56                          ; 0xc1362
    push di                                   ; 57                          ; 0xc1363
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1364
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1367
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc136a vgabios.c:838
    jnbe short 01384h                         ; 77 16                       ; 0xc136c
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc136e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1371
    mov es, ax                                ; 8e c0                       ; 0xc1374
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1376
    xor ah, ah                                ; 30 e4                       ; 0xc1379 vgabios.c:842
    call 03940h                               ; e8 c2 25                    ; 0xc137b
    mov cl, al                                ; 88 c1                       ; 0xc137e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1380 vgabios.c:843
    jne short 01387h                          ; 75 03                       ; 0xc1382
    jmp near 01427h                           ; e9 a0 00                    ; 0xc1384
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1387 vgabios.c:846
    xor ah, ah                                ; 30 e4                       ; 0xc138a
    lea bx, [bp-010h]                         ; 8d 5e f0                    ; 0xc138c
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc138f
    call 00a97h                               ; e8 02 f7                    ; 0xc1392
    mov bl, cl                                ; 88 cb                       ; 0xc1395 vgabios.c:848
    xor bh, bh                                ; 30 ff                       ; 0xc1397
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1399
    mov si, bx                                ; 89 de                       ; 0xc139b
    sal si, CL                                ; d3 e6                       ; 0xc139d
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc139f
    jne short 013e1h                          ; 75 3b                       ; 0xc13a4
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc13a6 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13a9
    mov es, ax                                ; 8e c0                       ; 0xc13ac
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc13ae
    mov bx, 00084h                            ; bb 84 00                    ; 0xc13b1 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc13b4
    xor ah, ah                                ; 30 e4                       ; 0xc13b7 vgabios.c:48
    mov bx, ax                                ; 89 c3                       ; 0xc13b9
    inc bx                                    ; 43                          ; 0xc13bb
    mov ax, dx                                ; 89 d0                       ; 0xc13bc vgabios.c:855
    mul bx                                    ; f7 e3                       ; 0xc13be
    mov di, ax                                ; 89 c7                       ; 0xc13c0
    sal ax, 1                                 ; d1 e0                       ; 0xc13c2
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc13c4
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc13c6
    xor bh, bh                                ; 30 ff                       ; 0xc13c9
    inc ax                                    ; 40                          ; 0xc13cb
    mul bx                                    ; f7 e3                       ; 0xc13cc
    mov cx, ax                                ; 89 c1                       ; 0xc13ce
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc13d0 vgabios.c:62
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc13d3
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc13d6 vgabios.c:859
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc13da
    mul bx                                    ; f7 e3                       ; 0xc13dd
    jmp short 013f2h                          ; eb 11                       ; 0xc13df vgabios.c:861
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc13e1 vgabios.c:863
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc13e5
    sal bx, CL                                ; d3 e3                       ; 0xc13e7
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc13e9
    xor ah, ah                                ; 30 e4                       ; 0xc13ec
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc13ee
    mov cx, ax                                ; 89 c1                       ; 0xc13f2
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc13f4 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13f7
    mov es, ax                                ; 8e c0                       ; 0xc13fa
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc13fc
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc13ff vgabios.c:868
    mov dx, bx                                ; 89 da                       ; 0xc1401
    out DX, AL                                ; ee                          ; 0xc1403
    mov al, ch                                ; 88 e8                       ; 0xc1404 vgabios.c:869
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc1406
    mov dx, si                                ; 89 f2                       ; 0xc1409
    out DX, AL                                ; ee                          ; 0xc140b
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc140c vgabios.c:870
    mov dx, bx                                ; 89 da                       ; 0xc140e
    out DX, AL                                ; ee                          ; 0xc1410
    xor ch, ch                                ; 30 ed                       ; 0xc1411 vgabios.c:871
    mov ax, cx                                ; 89 c8                       ; 0xc1413
    mov dx, si                                ; 89 f2                       ; 0xc1415
    out DX, AL                                ; ee                          ; 0xc1417
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1418 vgabios.c:52
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc141b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc141e
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc1421 vgabios.c:881
    call 012cfh                               ; e8 a8 fe                    ; 0xc1424
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1427 vgabios.c:882
    pop di                                    ; 5f                          ; 0xc142a
    pop si                                    ; 5e                          ; 0xc142b
    pop dx                                    ; 5a                          ; 0xc142c
    pop cx                                    ; 59                          ; 0xc142d
    pop bx                                    ; 5b                          ; 0xc142e
    pop bp                                    ; 5d                          ; 0xc142f
    retn                                      ; c3                          ; 0xc1430
  ; disGetNextSymbol 0xc1431 LB 0x31dc -> off=0x0 cb=0000000000000048 uValue=00000000000c1431 'find_vpti'
find_vpti:                                   ; 0xc1431 LB 0x48
    push bx                                   ; 53                          ; 0xc1431 vgabios.c:917
    push cx                                   ; 51                          ; 0xc1432
    push si                                   ; 56                          ; 0xc1433
    push bp                                   ; 55                          ; 0xc1434
    mov bp, sp                                ; 89 e5                       ; 0xc1435
    mov bl, al                                ; 88 c3                       ; 0xc1437 vgabios.c:922
    xor bh, bh                                ; 30 ff                       ; 0xc1439
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc143b
    mov si, bx                                ; 89 de                       ; 0xc143d
    sal si, CL                                ; d3 e6                       ; 0xc143f
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc1441
    jne short 0146eh                          ; 75 26                       ; 0xc1446
    mov si, 00089h                            ; be 89 00                    ; 0xc1448 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc144b
    mov es, ax                                ; 8e c0                       ; 0xc144e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1450
    test AL, strict byte 010h                 ; a8 10                       ; 0xc1453 vgabios.c:924
    je short 0145dh                           ; 74 06                       ; 0xc1455
    mov al, byte [bx+07df5h]                  ; 8a 87 f5 7d                 ; 0xc1457 vgabios.c:925
    jmp short 0146bh                          ; eb 0e                       ; 0xc145b vgabios.c:926
    test AL, strict byte 080h                 ; a8 80                       ; 0xc145d
    je short 01467h                           ; 74 06                       ; 0xc145f
    mov al, byte [bx+07de5h]                  ; 8a 87 e5 7d                 ; 0xc1461 vgabios.c:927
    jmp short 0146bh                          ; eb 04                       ; 0xc1465 vgabios.c:928
    mov al, byte [bx+07dedh]                  ; 8a 87 ed 7d                 ; 0xc1467 vgabios.c:929
    cbw                                       ; 98                          ; 0xc146b
    jmp short 01474h                          ; eb 06                       ; 0xc146c vgabios.c:930
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc146e vgabios.c:931
    xor ah, ah                                ; 30 e4                       ; 0xc1472
    pop bp                                    ; 5d                          ; 0xc1474 vgabios.c:934
    pop si                                    ; 5e                          ; 0xc1475
    pop cx                                    ; 59                          ; 0xc1476
    pop bx                                    ; 5b                          ; 0xc1477
    retn                                      ; c3                          ; 0xc1478
  ; disGetNextSymbol 0xc1479 LB 0x3194 -> off=0x0 cb=00000000000004b6 uValue=00000000000c1479 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc1479 LB 0x4b6
    push bp                                   ; 55                          ; 0xc1479 vgabios.c:938
    mov bp, sp                                ; 89 e5                       ; 0xc147a
    push bx                                   ; 53                          ; 0xc147c
    push cx                                   ; 51                          ; 0xc147d
    push dx                                   ; 52                          ; 0xc147e
    push si                                   ; 56                          ; 0xc147f
    push di                                   ; 57                          ; 0xc1480
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1481
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1484
    and AL, strict byte 080h                  ; 24 80                       ; 0xc1487 vgabios.c:942
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1489
    call 007f8h                               ; e8 69 f3                    ; 0xc148c vgabios.c:952
    test ax, ax                               ; 85 c0                       ; 0xc148f
    je short 0149fh                           ; 74 0c                       ; 0xc1491
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1493 vgabios.c:954
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1495
    out DX, AL                                ; ee                          ; 0xc1498
    xor al, al                                ; 30 c0                       ; 0xc1499 vgabios.c:955
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc149b
    out DX, AL                                ; ee                          ; 0xc149e
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc149f vgabios.c:960
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc14a3 vgabios.c:966
    xor ah, ah                                ; 30 e4                       ; 0xc14a6
    call 03940h                               ; e8 95 24                    ; 0xc14a8
    mov dl, al                                ; 88 c2                       ; 0xc14ab
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc14ad
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc14b0 vgabios.c:972
    je short 01520h                           ; 74 6c                       ; 0xc14b2
    mov si, 000a8h                            ; be a8 00                    ; 0xc14b4 vgabios.c:67
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc14b7
    mov es, ax                                ; 8e c0                       ; 0xc14ba
    mov bx, word [es:si]                      ; 26 8b 1c                    ; 0xc14bc
    mov ax, word [es:si+002h]                 ; 26 8b 44 02                 ; 0xc14bf
    mov word [bp-014h], bx                    ; 89 5e ec                    ; 0xc14c3 vgabios.c:68
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc14c6
    xor dh, dh                                ; 30 f6                       ; 0xc14c9 vgabios.c:978
    mov ax, dx                                ; 89 d0                       ; 0xc14cb
    call 01431h                               ; e8 61 ff                    ; 0xc14cd
    mov es, [bp-012h]                         ; 8e 46 ee                    ; 0xc14d0 vgabios.c:979
    mov si, word [es:bx]                      ; 26 8b 37                    ; 0xc14d3
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc14d6
    mov word [bp-01ah], bx                    ; 89 5e e6                    ; 0xc14da
    xor ah, ah                                ; 30 e4                       ; 0xc14dd vgabios.c:980
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc14df
    sal ax, CL                                ; d3 e0                       ; 0xc14e1
    add si, ax                                ; 01 c6                       ; 0xc14e3
    mov bx, 00089h                            ; bb 89 00                    ; 0xc14e5 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc14e8
    mov es, ax                                ; 8e c0                       ; 0xc14eb
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc14ed
    mov ch, al                                ; 88 c5                       ; 0xc14f0 vgabios.c:48
    test AL, strict byte 008h                 ; a8 08                       ; 0xc14f2 vgabios.c:997
    jne short 0153ch                          ; 75 46                       ; 0xc14f4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc14f6 vgabios.c:999
    mov bx, dx                                ; 89 d3                       ; 0xc14f8
    sal bx, CL                                ; d3 e3                       ; 0xc14fa
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc14fc
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1500
    out DX, AL                                ; ee                          ; 0xc1503
    xor al, al                                ; 30 c0                       ; 0xc1504 vgabios.c:1002
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1506
    out DX, AL                                ; ee                          ; 0xc1509
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc150a vgabios.c:1005
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc150e
    jc short 01523h                           ; 72 10                       ; 0xc1511
    jbe short 0152eh                          ; 76 19                       ; 0xc1513
    cmp bl, cl                                ; 38 cb                       ; 0xc1515
    je short 0153fh                           ; 74 26                       ; 0xc1517
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1519
    je short 01535h                           ; 74 17                       ; 0xc151c
    jmp short 01544h                          ; eb 24                       ; 0xc151e
    jmp near 01925h                           ; e9 02 04                    ; 0xc1520
    test bl, bl                               ; 84 db                       ; 0xc1523
    jne short 01544h                          ; 75 1d                       ; 0xc1525
    mov word [bp-016h], 04fc2h                ; c7 46 ea c2 4f              ; 0xc1527 vgabios.c:1007
    jmp short 01544h                          ; eb 16                       ; 0xc152c vgabios.c:1008
    mov word [bp-016h], 05082h                ; c7 46 ea 82 50              ; 0xc152e vgabios.c:1010
    jmp short 01544h                          ; eb 0f                       ; 0xc1533 vgabios.c:1011
    mov word [bp-016h], 05142h                ; c7 46 ea 42 51              ; 0xc1535 vgabios.c:1013
    jmp short 01544h                          ; eb 08                       ; 0xc153a vgabios.c:1014
    jmp near 015b8h                           ; e9 79 00                    ; 0xc153c
    mov word [bp-016h], 05202h                ; c7 46 ea 02 52              ; 0xc153f vgabios.c:1016
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1544 vgabios.c:1020
    xor bh, bh                                ; 30 ff                       ; 0xc1547
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1549
    sal bx, CL                                ; d3 e3                       ; 0xc154b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc154d
    jne short 01563h                          ; 75 0f                       ; 0xc1552
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1554 vgabios.c:1022
    cmp byte [es:si+002h], 008h               ; 26 80 7c 02 08              ; 0xc1557
    jne short 01563h                          ; 75 05                       ; 0xc155c
    mov word [bp-016h], 05082h                ; c7 46 ea 82 50              ; 0xc155e vgabios.c:1023
    xor bx, bx                                ; 31 db                       ; 0xc1563 vgabios.c:1026
    jmp short 01576h                          ; eb 0f                       ; 0xc1565
    xor al, al                                ; 30 c0                       ; 0xc1567 vgabios.c:1033
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1569
    out DX, AL                                ; ee                          ; 0xc156c
    out DX, AL                                ; ee                          ; 0xc156d vgabios.c:1034
    out DX, AL                                ; ee                          ; 0xc156e vgabios.c:1035
    inc bx                                    ; 43                          ; 0xc156f vgabios.c:1037
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc1570
    jnc short 015abh                          ; 73 35                       ; 0xc1574
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1576
    xor ah, ah                                ; 30 e4                       ; 0xc1579
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc157b
    mov di, ax                                ; 89 c7                       ; 0xc157d
    sal di, CL                                ; d3 e7                       ; 0xc157f
    mov al, byte [di+047b5h]                  ; 8a 85 b5 47                 ; 0xc1581
    mov di, ax                                ; 89 c7                       ; 0xc1585
    mov al, byte [di+0483eh]                  ; 8a 85 3e 48                 ; 0xc1587
    cmp bx, ax                                ; 39 c3                       ; 0xc158b
    jnbe short 01567h                         ; 77 d8                       ; 0xc158d
    mov ax, bx                                ; 89 d8                       ; 0xc158f
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc1591
    mul dx                                    ; f7 e2                       ; 0xc1594
    mov di, word [bp-016h]                    ; 8b 7e ea                    ; 0xc1596
    add di, ax                                ; 01 c7                       ; 0xc1599
    mov al, byte [di]                         ; 8a 05                       ; 0xc159b
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc159d
    out DX, AL                                ; ee                          ; 0xc15a0
    mov al, byte [di+001h]                    ; 8a 45 01                    ; 0xc15a1
    out DX, AL                                ; ee                          ; 0xc15a4
    mov al, byte [di+002h]                    ; 8a 45 02                    ; 0xc15a5
    out DX, AL                                ; ee                          ; 0xc15a8
    jmp short 0156fh                          ; eb c4                       ; 0xc15a9
    test ch, 002h                             ; f6 c5 02                    ; 0xc15ab vgabios.c:1038
    je short 015b8h                           ; 74 08                       ; 0xc15ae
    mov dx, 00100h                            ; ba 00 01                    ; 0xc15b0 vgabios.c:1040
    xor ax, ax                                ; 31 c0                       ; 0xc15b3
    call 01134h                               ; e8 7c fb                    ; 0xc15b5
    mov dx, 003dah                            ; ba da 03                    ; 0xc15b8 vgabios.c:1045
    in AL, DX                                 ; ec                          ; 0xc15bb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc15bc
    xor bx, bx                                ; 31 db                       ; 0xc15be vgabios.c:1048
    jmp short 015c7h                          ; eb 05                       ; 0xc15c0
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc15c2
    jnbe short 015dch                         ; 77 15                       ; 0xc15c5
    mov al, bl                                ; 88 d8                       ; 0xc15c7 vgabios.c:1049
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc15c9
    out DX, AL                                ; ee                          ; 0xc15cc
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc15cd vgabios.c:1050
    mov di, si                                ; 89 f7                       ; 0xc15d0
    add di, bx                                ; 01 df                       ; 0xc15d2
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc15d4
    out DX, AL                                ; ee                          ; 0xc15d8
    inc bx                                    ; 43                          ; 0xc15d9 vgabios.c:1051
    jmp short 015c2h                          ; eb e6                       ; 0xc15da
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc15dc vgabios.c:1052
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc15de
    out DX, AL                                ; ee                          ; 0xc15e1
    xor al, al                                ; 30 c0                       ; 0xc15e2 vgabios.c:1053
    out DX, AL                                ; ee                          ; 0xc15e4
    les bx, [bp-014h]                         ; c4 5e ec                    ; 0xc15e5 vgabios.c:1056
    mov dx, word [es:bx+004h]                 ; 26 8b 57 04                 ; 0xc15e8
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06                 ; 0xc15ec
    test ax, ax                               ; 85 c0                       ; 0xc15f0
    jne short 015f8h                          ; 75 04                       ; 0xc15f2
    test dx, dx                               ; 85 d2                       ; 0xc15f4
    je short 01634h                           ; 74 3c                       ; 0xc15f6
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc15f8 vgabios.c:1060
    xor bx, bx                                ; 31 db                       ; 0xc15fb vgabios.c:1061
    jmp short 01604h                          ; eb 05                       ; 0xc15fd
    cmp bx, strict byte 00010h                ; 83 fb 10                    ; 0xc15ff
    jnc short 01624h                          ; 73 20                       ; 0xc1602
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1604 vgabios.c:1062
    mov di, si                                ; 89 f7                       ; 0xc1607
    add di, bx                                ; 01 df                       ; 0xc1609
    mov ax, word [bp-020h]                    ; 8b 46 e0                    ; 0xc160b
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc160e
    mov cx, dx                                ; 89 d1                       ; 0xc1611
    add cx, bx                                ; 01 d9                       ; 0xc1613
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc1615
    mov es, [bp-022h]                         ; 8e 46 de                    ; 0xc1619
    mov di, cx                                ; 89 cf                       ; 0xc161c
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc161e
    inc bx                                    ; 43                          ; 0xc1621
    jmp short 015ffh                          ; eb db                       ; 0xc1622
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1624 vgabios.c:1063
    mov al, byte [es:si+034h]                 ; 26 8a 44 34                 ; 0xc1627
    mov es, [bp-020h]                         ; 8e 46 e0                    ; 0xc162b
    mov bx, dx                                ; 89 d3                       ; 0xc162e
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc1630
    xor al, al                                ; 30 c0                       ; 0xc1634 vgabios.c:1067
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1636
    out DX, AL                                ; ee                          ; 0xc1639
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc163a vgabios.c:1068
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc163c
    out DX, AL                                ; ee                          ; 0xc163f
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc1640 vgabios.c:1069
    jmp short 0164ah                          ; eb 05                       ; 0xc1643
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc1645
    jnbe short 01662h                         ; 77 18                       ; 0xc1648
    mov al, bl                                ; 88 d8                       ; 0xc164a vgabios.c:1070
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc164c
    out DX, AL                                ; ee                          ; 0xc164f
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1650 vgabios.c:1071
    mov di, si                                ; 89 f7                       ; 0xc1653
    add di, bx                                ; 01 df                       ; 0xc1655
    mov al, byte [es:di+004h]                 ; 26 8a 45 04                 ; 0xc1657
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc165b
    out DX, AL                                ; ee                          ; 0xc165e
    inc bx                                    ; 43                          ; 0xc165f vgabios.c:1072
    jmp short 01645h                          ; eb e3                       ; 0xc1660
    xor bx, bx                                ; 31 db                       ; 0xc1662 vgabios.c:1075
    jmp short 0166bh                          ; eb 05                       ; 0xc1664
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1666
    jnbe short 01683h                         ; 77 18                       ; 0xc1669
    mov al, bl                                ; 88 d8                       ; 0xc166b vgabios.c:1076
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc166d
    out DX, AL                                ; ee                          ; 0xc1670
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1671 vgabios.c:1077
    mov di, si                                ; 89 f7                       ; 0xc1674
    add di, bx                                ; 01 df                       ; 0xc1676
    mov al, byte [es:di+037h]                 ; 26 8a 45 37                 ; 0xc1678
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc167c
    out DX, AL                                ; ee                          ; 0xc167f
    inc bx                                    ; 43                          ; 0xc1680 vgabios.c:1078
    jmp short 01666h                          ; eb e3                       ; 0xc1681
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1683 vgabios.c:1081
    xor bh, bh                                ; 30 ff                       ; 0xc1686
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1688
    sal bx, CL                                ; d3 e3                       ; 0xc168a
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc168c
    jne short 01698h                          ; 75 05                       ; 0xc1691
    mov bx, 003b4h                            ; bb b4 03                    ; 0xc1693
    jmp short 0169bh                          ; eb 03                       ; 0xc1696
    mov bx, 003d4h                            ; bb d4 03                    ; 0xc1698
    mov word [bp-018h], bx                    ; 89 5e e8                    ; 0xc169b
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc169e vgabios.c:1084
    mov al, byte [es:si+009h]                 ; 26 8a 44 09                 ; 0xc16a1
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc16a5
    out DX, AL                                ; ee                          ; 0xc16a8
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc16a9 vgabios.c:1087
    mov dx, bx                                ; 89 da                       ; 0xc16ac
    out DX, ax                                ; ef                          ; 0xc16ae
    xor bx, bx                                ; 31 db                       ; 0xc16af vgabios.c:1089
    jmp short 016b8h                          ; eb 05                       ; 0xc16b1
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc16b3
    jnbe short 016ceh                         ; 77 16                       ; 0xc16b6
    mov al, bl                                ; 88 d8                       ; 0xc16b8 vgabios.c:1090
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc16ba
    out DX, AL                                ; ee                          ; 0xc16bd
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc16be vgabios.c:1091
    mov di, si                                ; 89 f7                       ; 0xc16c1
    add di, bx                                ; 01 df                       ; 0xc16c3
    inc dx                                    ; 42                          ; 0xc16c5
    mov al, byte [es:di+00ah]                 ; 26 8a 45 0a                 ; 0xc16c6
    out DX, AL                                ; ee                          ; 0xc16ca
    inc bx                                    ; 43                          ; 0xc16cb vgabios.c:1092
    jmp short 016b3h                          ; eb e5                       ; 0xc16cc
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc16ce vgabios.c:1095
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc16d0
    out DX, AL                                ; ee                          ; 0xc16d3
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc16d4 vgabios.c:1096
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc16d7
    in AL, DX                                 ; ec                          ; 0xc16da
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc16db
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc16dd vgabios.c:1098
    jne short 01741h                          ; 75 5e                       ; 0xc16e1
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16e3 vgabios.c:1100
    xor bh, bh                                ; 30 ff                       ; 0xc16e6
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc16e8
    sal bx, CL                                ; d3 e3                       ; 0xc16ea
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16ec
    jne short 01705h                          ; 75 12                       ; 0xc16f1
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc16f3 vgabios.c:1102
    mov cx, 04000h                            ; b9 00 40                    ; 0xc16f7
    mov ax, 00720h                            ; b8 20 07                    ; 0xc16fa
    xor di, di                                ; 31 ff                       ; 0xc16fd
    jcxz 01703h                               ; e3 02                       ; 0xc16ff
    rep stosw                                 ; f3 ab                       ; 0xc1701
    jmp short 01741h                          ; eb 3c                       ; 0xc1703 vgabios.c:1104
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc1705 vgabios.c:1106
    jnc short 0171ch                          ; 73 11                       ; 0xc1709
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc170b vgabios.c:1108
    mov cx, 04000h                            ; b9 00 40                    ; 0xc170f
    xor ax, ax                                ; 31 c0                       ; 0xc1712
    xor di, di                                ; 31 ff                       ; 0xc1714
    jcxz 0171ah                               ; e3 02                       ; 0xc1716
    rep stosw                                 ; f3 ab                       ; 0xc1718
    jmp short 01741h                          ; eb 25                       ; 0xc171a vgabios.c:1110
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc171c vgabios.c:1112
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc171e
    out DX, AL                                ; ee                          ; 0xc1721
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1722 vgabios.c:1113
    in AL, DX                                 ; ec                          ; 0xc1725
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1726
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1728
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc172b vgabios.c:1114
    out DX, AL                                ; ee                          ; 0xc172d
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc172e vgabios.c:1115
    mov cx, 08000h                            ; b9 00 80                    ; 0xc1732
    xor ax, ax                                ; 31 c0                       ; 0xc1735
    xor di, di                                ; 31 ff                       ; 0xc1737
    jcxz 0173dh                               ; e3 02                       ; 0xc1739
    rep stosw                                 ; f3 ab                       ; 0xc173b
    mov al, byte [bp-022h]                    ; 8a 46 de                    ; 0xc173d vgabios.c:1116
    out DX, AL                                ; ee                          ; 0xc1740
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1741 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1744
    mov es, ax                                ; 8e c0                       ; 0xc1747
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1749
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc174c
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc174f vgabios.c:1123
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1752
    xor ah, ah                                ; 30 e4                       ; 0xc1755
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1757 vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc175a
    mov es, dx                                ; 8e c2                       ; 0xc175d
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc175f
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1762 vgabios.c:60
    mov ax, word [es:si+003h]                 ; 26 8b 44 03                 ; 0xc1765
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc1769 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc176c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc176e
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1771 vgabios.c:62
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1774
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1777
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc177a vgabios.c:50
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc177d
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1781 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc1784
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1786
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1789 vgabios.c:1127
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc178c
    xor ah, ah                                ; 30 e4                       ; 0xc1790
    mov bx, 00085h                            ; bb 85 00                    ; 0xc1792 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc1795
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1797
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc179a vgabios.c:1128
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc179d
    mov bx, 00087h                            ; bb 87 00                    ; 0xc179f vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc17a2
    mov bx, 00088h                            ; bb 88 00                    ; 0xc17a5 vgabios.c:52
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc17a8
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc17ac vgabios.c:52
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc17af
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc17b3 vgabios.c:1134
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc17b6
    jnbe short 017e1h                         ; 77 27                       ; 0xc17b8
    mov bl, al                                ; 88 c3                       ; 0xc17ba vgabios.c:1136
    xor bh, bh                                ; 30 ff                       ; 0xc17bc
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc17be vgabios.c:50
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc17c2 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc17c5
    cmp byte [bp-00ch], 006h                  ; 80 7e f4 06                 ; 0xc17c8 vgabios.c:1137
    jne short 017d3h                          ; 75 05                       ; 0xc17cc
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc17ce
    jmp short 017d6h                          ; eb 03                       ; 0xc17d1
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc17d3
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc17d6 vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc17d9
    mov es, dx                                ; 8e c2                       ; 0xc17dc
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc17de
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc17e1 vgabios.c:1141
    xor bh, bh                                ; 30 ff                       ; 0xc17e4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc17e6
    sal bx, CL                                ; d3 e3                       ; 0xc17e8
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc17ea
    jne short 017fah                          ; 75 09                       ; 0xc17ef
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc17f1 vgabios.c:1143
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc17f4
    call 011d3h                               ; e8 d9 f9                    ; 0xc17f7
    xor bx, bx                                ; 31 db                       ; 0xc17fa vgabios.c:1148
    jmp short 01803h                          ; eb 05                       ; 0xc17fc
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc17fe
    jnc short 0180fh                          ; 73 0c                       ; 0xc1801
    mov al, bl                                ; 88 d8                       ; 0xc1803 vgabios.c:1149
    xor ah, ah                                ; 30 e4                       ; 0xc1805
    xor dx, dx                                ; 31 d2                       ; 0xc1807
    call 012cfh                               ; e8 c3 fa                    ; 0xc1809
    inc bx                                    ; 43                          ; 0xc180c
    jmp short 017feh                          ; eb ef                       ; 0xc180d
    xor ax, ax                                ; 31 c0                       ; 0xc180f vgabios.c:1152
    call 0135ch                               ; e8 48 fb                    ; 0xc1811
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1814 vgabios.c:1155
    xor bh, bh                                ; 30 ff                       ; 0xc1817
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1819
    sal bx, CL                                ; d3 e3                       ; 0xc181b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc181d
    jne short 01872h                          ; 75 4e                       ; 0xc1822
    les bx, [bp-014h]                         ; c4 5e ec                    ; 0xc1824 vgabios.c:1157
    mov bx, word [es:bx+008h]                 ; 26 8b 5f 08                 ; 0xc1827
    mov word [bp-01eh], bx                    ; 89 5e e2                    ; 0xc182b
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc182e
    mov ax, word [es:bx+00ah]                 ; 26 8b 47 0a                 ; 0xc1831
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1835
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1838 vgabios.c:1159
    mov bl, byte [es:si+002h]                 ; 26 8a 5c 02                 ; 0xc183b
    cmp bl, 00eh                              ; 80 fb 0e                    ; 0xc183f
    je short 0185fh                           ; 74 1b                       ; 0xc1842
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc1844
    jne short 01875h                          ; 75 2c                       ; 0xc1847
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1849 vgabios.c:1161
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc184c
    xor ah, ah                                ; 30 e4                       ; 0xc1850
    push ax                                   ; 50                          ; 0xc1852
    xor al, al                                ; 30 c0                       ; 0xc1853
    push ax                                   ; 50                          ; 0xc1855
    push ax                                   ; 50                          ; 0xc1856
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1857
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc185a
    jmp short 01886h                          ; eb 27                       ; 0xc185d vgabios.c:1162
    mov al, bl                                ; 88 d8                       ; 0xc185f vgabios.c:1164
    xor ah, ah                                ; 30 e4                       ; 0xc1861
    push ax                                   ; 50                          ; 0xc1863
    xor al, bl                                ; 30 d8                       ; 0xc1864
    push ax                                   ; 50                          ; 0xc1866
    xor al, al                                ; 30 c0                       ; 0xc1867
    push ax                                   ; 50                          ; 0xc1869
    mov cx, 00100h                            ; b9 00 01                    ; 0xc186a
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc186d
    jmp short 01886h                          ; eb 14                       ; 0xc1870
    jmp near 018edh                           ; e9 78 00                    ; 0xc1872
    mov al, bl                                ; 88 d8                       ; 0xc1875 vgabios.c:1167
    xor ah, ah                                ; 30 e4                       ; 0xc1877
    push ax                                   ; 50                          ; 0xc1879
    xor al, bl                                ; 30 d8                       ; 0xc187a
    push ax                                   ; 50                          ; 0xc187c
    xor al, al                                ; 30 c0                       ; 0xc187d
    push ax                                   ; 50                          ; 0xc187f
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1880
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc1883
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc1886
    call 02de9h                               ; e8 5d 15                    ; 0xc1889
    cmp word [bp-01ch], strict byte 00000h    ; 83 7e e4 00                 ; 0xc188c vgabios.c:1169
    jne short 01898h                          ; 75 06                       ; 0xc1890
    cmp word [bp-01eh], strict byte 00000h    ; 83 7e e2 00                 ; 0xc1892
    je short 018e5h                           ; 74 4d                       ; 0xc1896
    xor bx, bx                                ; 31 db                       ; 0xc1898 vgabios.c:1174
    les di, [bp-01eh]                         ; c4 7e e2                    ; 0xc189a vgabios.c:1176
    add di, bx                                ; 01 df                       ; 0xc189d
    mov al, byte [es:di+00bh]                 ; 26 8a 45 0b                 ; 0xc189f
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc18a3
    je short 018afh                           ; 74 08                       ; 0xc18a5
    cmp al, byte [bp-00ch]                    ; 3a 46 f4                    ; 0xc18a7 vgabios.c:1178
    je short 018afh                           ; 74 03                       ; 0xc18aa
    inc bx                                    ; 43                          ; 0xc18ac vgabios.c:1180
    jmp short 0189ah                          ; eb eb                       ; 0xc18ad vgabios.c:1181
    mov es, [bp-01ch]                         ; 8e 46 e4                    ; 0xc18af vgabios.c:1183
    add bx, word [bp-01eh]                    ; 03 5e e2                    ; 0xc18b2
    mov al, byte [es:bx+00bh]                 ; 26 8a 47 0b                 ; 0xc18b5
    cmp al, byte [bp-00ch]                    ; 3a 46 f4                    ; 0xc18b9
    jne short 018e5h                          ; 75 27                       ; 0xc18bc
    mov bx, word [bp-01eh]                    ; 8b 5e e2                    ; 0xc18be vgabios.c:1188
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc18c1
    xor ah, ah                                ; 30 e4                       ; 0xc18c4
    push ax                                   ; 50                          ; 0xc18c6
    mov al, byte [es:bx+001h]                 ; 26 8a 47 01                 ; 0xc18c7
    push ax                                   ; 50                          ; 0xc18cb
    push word [es:bx+004h]                    ; 26 ff 77 04                 ; 0xc18cc
    mov cx, word [es:bx+002h]                 ; 26 8b 4f 02                 ; 0xc18d0
    mov bx, word [es:bx+006h]                 ; 26 8b 5f 06                 ; 0xc18d4
    mov di, word [bp-01eh]                    ; 8b 7e e2                    ; 0xc18d8
    mov dx, word [es:di+008h]                 ; 26 8b 55 08                 ; 0xc18db
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc18df
    call 02de9h                               ; e8 04 15                    ; 0xc18e2
    xor bl, bl                                ; 30 db                       ; 0xc18e5 vgabios.c:1192
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc18e7
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc18e9
    int 06dh                                  ; cd 6d                       ; 0xc18eb
    mov bx, 0596ch                            ; bb 6c 59                    ; 0xc18ed vgabios.c:1196
    mov cx, ds                                ; 8c d9                       ; 0xc18f0
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc18f2
    call 009f0h                               ; e8 f8 f0                    ; 0xc18f5
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc18f8 vgabios.c:1198
    mov dl, byte [es:si+002h]                 ; 26 8a 54 02                 ; 0xc18fb
    cmp dl, 010h                              ; 80 fa 10                    ; 0xc18ff
    je short 01920h                           ; 74 1c                       ; 0xc1902
    cmp dl, 00eh                              ; 80 fa 0e                    ; 0xc1904
    je short 0191bh                           ; 74 12                       ; 0xc1907
    cmp dl, 008h                              ; 80 fa 08                    ; 0xc1909
    jne short 01925h                          ; 75 17                       ; 0xc190c
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc190e vgabios.c:1200
    mov cx, ds                                ; 8c d9                       ; 0xc1911
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc1913
    call 009f0h                               ; e8 d7 f0                    ; 0xc1916
    jmp short 01925h                          ; eb 0a                       ; 0xc1919 vgabios.c:1201
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc191b vgabios.c:1203
    jmp short 01911h                          ; eb f1                       ; 0xc191e
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc1920 vgabios.c:1206
    jmp short 01911h                          ; eb ec                       ; 0xc1923
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1925 vgabios.c:1209
    pop di                                    ; 5f                          ; 0xc1928
    pop si                                    ; 5e                          ; 0xc1929
    pop dx                                    ; 5a                          ; 0xc192a
    pop cx                                    ; 59                          ; 0xc192b
    pop bx                                    ; 5b                          ; 0xc192c
    pop bp                                    ; 5d                          ; 0xc192d
    retn                                      ; c3                          ; 0xc192e
  ; disGetNextSymbol 0xc192f LB 0x2cde -> off=0x0 cb=000000000000008e uValue=00000000000c192f 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc192f LB 0x8e
    push bp                                   ; 55                          ; 0xc192f vgabios.c:1212
    mov bp, sp                                ; 89 e5                       ; 0xc1930
    push si                                   ; 56                          ; 0xc1932
    push di                                   ; 57                          ; 0xc1933
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1934
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1937
    mov al, dl                                ; 88 d0                       ; 0xc193a
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc193c
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc193f
    xor ah, ah                                ; 30 e4                       ; 0xc1942 vgabios.c:1218
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1944
    xor dh, dh                                ; 30 f6                       ; 0xc1947
    mov cx, dx                                ; 89 d1                       ; 0xc1949
    imul dx                                   ; f7 ea                       ; 0xc194b
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc194d
    xor dh, dh                                ; 30 f6                       ; 0xc1950
    mov si, dx                                ; 89 d6                       ; 0xc1952
    imul dx                                   ; f7 ea                       ; 0xc1954
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1956
    xor dh, dh                                ; 30 f6                       ; 0xc1959
    mov bx, dx                                ; 89 d3                       ; 0xc195b
    add ax, dx                                ; 01 d0                       ; 0xc195d
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc195f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1962 vgabios.c:1219
    xor ah, ah                                ; 30 e4                       ; 0xc1965
    imul cx                                   ; f7 e9                       ; 0xc1967
    imul si                                   ; f7 ee                       ; 0xc1969
    add ax, bx                                ; 01 d8                       ; 0xc196b
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc196d
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1970 vgabios.c:1220
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1973
    out DX, ax                                ; ef                          ; 0xc1976
    xor bl, bl                                ; 30 db                       ; 0xc1977 vgabios.c:1221
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1979
    jnc short 019adh                          ; 73 2f                       ; 0xc197c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc197e vgabios.c:1223
    xor ah, ah                                ; 30 e4                       ; 0xc1981
    mov cx, ax                                ; 89 c1                       ; 0xc1983
    mov al, bl                                ; 88 d8                       ; 0xc1985
    mov dx, ax                                ; 89 c2                       ; 0xc1987
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1989
    mov si, ax                                ; 89 c6                       ; 0xc198c
    mov ax, dx                                ; 89 d0                       ; 0xc198e
    imul si                                   ; f7 ee                       ; 0xc1990
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1992
    add si, ax                                ; 01 c6                       ; 0xc1995
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1997
    add di, ax                                ; 01 c7                       ; 0xc199a
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc199c
    mov es, dx                                ; 8e c2                       ; 0xc199f
    jcxz 019a9h                               ; e3 06                       ; 0xc19a1
    push DS                                   ; 1e                          ; 0xc19a3
    mov ds, dx                                ; 8e da                       ; 0xc19a4
    rep movsb                                 ; f3 a4                       ; 0xc19a6
    pop DS                                    ; 1f                          ; 0xc19a8
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc19a9 vgabios.c:1224
    jmp short 01979h                          ; eb cc                       ; 0xc19ab
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc19ad vgabios.c:1225
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc19b0
    out DX, ax                                ; ef                          ; 0xc19b3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc19b4 vgabios.c:1226
    pop di                                    ; 5f                          ; 0xc19b7
    pop si                                    ; 5e                          ; 0xc19b8
    pop bp                                    ; 5d                          ; 0xc19b9
    retn 00004h                               ; c2 04 00                    ; 0xc19ba
  ; disGetNextSymbol 0xc19bd LB 0x2c50 -> off=0x0 cb=000000000000007b uValue=00000000000c19bd 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc19bd LB 0x7b
    push bp                                   ; 55                          ; 0xc19bd vgabios.c:1229
    mov bp, sp                                ; 89 e5                       ; 0xc19be
    push si                                   ; 56                          ; 0xc19c0
    push di                                   ; 57                          ; 0xc19c1
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc19c2
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc19c5
    mov al, dl                                ; 88 d0                       ; 0xc19c8
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc19ca
    mov bh, cl                                ; 88 cf                       ; 0xc19cd
    xor ah, ah                                ; 30 e4                       ; 0xc19cf vgabios.c:1235
    mov dx, ax                                ; 89 c2                       ; 0xc19d1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc19d3
    mov cx, ax                                ; 89 c1                       ; 0xc19d6
    mov ax, dx                                ; 89 d0                       ; 0xc19d8
    imul cx                                   ; f7 e9                       ; 0xc19da
    mov dl, bh                                ; 88 fa                       ; 0xc19dc
    xor dh, dh                                ; 30 f6                       ; 0xc19de
    imul dx                                   ; f7 ea                       ; 0xc19e0
    mov dx, ax                                ; 89 c2                       ; 0xc19e2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19e4
    xor ah, ah                                ; 30 e4                       ; 0xc19e7
    add dx, ax                                ; 01 c2                       ; 0xc19e9
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc19eb
    mov ax, 00205h                            ; b8 05 02                    ; 0xc19ee vgabios.c:1236
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc19f1
    out DX, ax                                ; ef                          ; 0xc19f4
    xor bl, bl                                ; 30 db                       ; 0xc19f5 vgabios.c:1237
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc19f7
    jnc short 01a28h                          ; 73 2c                       ; 0xc19fa
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc19fc vgabios.c:1239
    xor ch, ch                                ; 30 ed                       ; 0xc19ff
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a01
    xor ah, ah                                ; 30 e4                       ; 0xc1a04
    mov si, ax                                ; 89 c6                       ; 0xc1a06
    mov al, bl                                ; 88 d8                       ; 0xc1a08
    mov dx, ax                                ; 89 c2                       ; 0xc1a0a
    mov al, bh                                ; 88 f8                       ; 0xc1a0c
    mov di, ax                                ; 89 c7                       ; 0xc1a0e
    mov ax, dx                                ; 89 d0                       ; 0xc1a10
    imul di                                   ; f7 ef                       ; 0xc1a12
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1a14
    add di, ax                                ; 01 c7                       ; 0xc1a17
    mov ax, si                                ; 89 f0                       ; 0xc1a19
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a1b
    mov es, dx                                ; 8e c2                       ; 0xc1a1e
    jcxz 01a24h                               ; e3 02                       ; 0xc1a20
    rep stosb                                 ; f3 aa                       ; 0xc1a22
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1a24 vgabios.c:1240
    jmp short 019f7h                          ; eb cf                       ; 0xc1a26
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1a28 vgabios.c:1241
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1a2b
    out DX, ax                                ; ef                          ; 0xc1a2e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a2f vgabios.c:1242
    pop di                                    ; 5f                          ; 0xc1a32
    pop si                                    ; 5e                          ; 0xc1a33
    pop bp                                    ; 5d                          ; 0xc1a34
    retn 00004h                               ; c2 04 00                    ; 0xc1a35
  ; disGetNextSymbol 0xc1a38 LB 0x2bd5 -> off=0x0 cb=00000000000000b6 uValue=00000000000c1a38 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1a38 LB 0xb6
    push bp                                   ; 55                          ; 0xc1a38 vgabios.c:1245
    mov bp, sp                                ; 89 e5                       ; 0xc1a39
    push si                                   ; 56                          ; 0xc1a3b
    push di                                   ; 57                          ; 0xc1a3c
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1a3d
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1a40
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1a43
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc1a46
    mov al, dl                                ; 88 d0                       ; 0xc1a49 vgabios.c:1251
    xor ah, ah                                ; 30 e4                       ; 0xc1a4b
    mov bx, ax                                ; 89 c3                       ; 0xc1a4d
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a4f
    mov si, ax                                ; 89 c6                       ; 0xc1a52
    mov ax, bx                                ; 89 d8                       ; 0xc1a54
    imul si                                   ; f7 ee                       ; 0xc1a56
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1a58
    mov di, bx                                ; 89 df                       ; 0xc1a5b
    imul bx                                   ; f7 eb                       ; 0xc1a5d
    mov dx, ax                                ; 89 c2                       ; 0xc1a5f
    sar dx, 1                                 ; d1 fa                       ; 0xc1a61
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1a63
    xor ah, ah                                ; 30 e4                       ; 0xc1a66
    mov bx, ax                                ; 89 c3                       ; 0xc1a68
    add dx, ax                                ; 01 c2                       ; 0xc1a6a
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1a6c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a6f vgabios.c:1252
    imul si                                   ; f7 ee                       ; 0xc1a72
    imul di                                   ; f7 ef                       ; 0xc1a74
    sar ax, 1                                 ; d1 f8                       ; 0xc1a76
    add ax, bx                                ; 01 d8                       ; 0xc1a78
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1a7a
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc1a7d vgabios.c:1253
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a80
    xor ah, ah                                ; 30 e4                       ; 0xc1a83
    cwd                                       ; 99                          ; 0xc1a85
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1a86
    sar ax, 1                                 ; d1 f8                       ; 0xc1a88
    mov bx, ax                                ; 89 c3                       ; 0xc1a8a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a8c
    xor ah, ah                                ; 30 e4                       ; 0xc1a8f
    cmp ax, bx                                ; 39 d8                       ; 0xc1a91
    jnl short 01ae5h                          ; 7d 50                       ; 0xc1a93
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1a95 vgabios.c:1255
    xor bh, bh                                ; 30 ff                       ; 0xc1a98
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc1a9a
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1a9d
    imul bx                                   ; f7 eb                       ; 0xc1aa0
    mov bx, ax                                ; 89 c3                       ; 0xc1aa2
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1aa4
    add si, ax                                ; 01 c6                       ; 0xc1aa7
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1aa9
    add di, ax                                ; 01 c7                       ; 0xc1aac
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1aae
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1ab1
    mov es, dx                                ; 8e c2                       ; 0xc1ab4
    jcxz 01abeh                               ; e3 06                       ; 0xc1ab6
    push DS                                   ; 1e                          ; 0xc1ab8
    mov ds, dx                                ; 8e da                       ; 0xc1ab9
    rep movsb                                 ; f3 a4                       ; 0xc1abb
    pop DS                                    ; 1f                          ; 0xc1abd
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1abe vgabios.c:1256
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc1ac1
    add si, bx                                ; 01 de                       ; 0xc1ac5
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1ac7
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1aca
    add di, bx                                ; 01 df                       ; 0xc1ace
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1ad0
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1ad3
    mov es, dx                                ; 8e c2                       ; 0xc1ad6
    jcxz 01ae0h                               ; e3 06                       ; 0xc1ad8
    push DS                                   ; 1e                          ; 0xc1ada
    mov ds, dx                                ; 8e da                       ; 0xc1adb
    rep movsb                                 ; f3 a4                       ; 0xc1add
    pop DS                                    ; 1f                          ; 0xc1adf
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1ae0 vgabios.c:1257
    jmp short 01a80h                          ; eb 9b                       ; 0xc1ae3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1ae5 vgabios.c:1258
    pop di                                    ; 5f                          ; 0xc1ae8
    pop si                                    ; 5e                          ; 0xc1ae9
    pop bp                                    ; 5d                          ; 0xc1aea
    retn 00004h                               ; c2 04 00                    ; 0xc1aeb
  ; disGetNextSymbol 0xc1aee LB 0x2b1f -> off=0x0 cb=0000000000000094 uValue=00000000000c1aee 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc1aee LB 0x94
    push bp                                   ; 55                          ; 0xc1aee vgabios.c:1261
    mov bp, sp                                ; 89 e5                       ; 0xc1aef
    push si                                   ; 56                          ; 0xc1af1
    push di                                   ; 57                          ; 0xc1af2
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1af3
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1af6
    mov al, dl                                ; 88 d0                       ; 0xc1af9
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1afb
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1afe
    xor ah, ah                                ; 30 e4                       ; 0xc1b01 vgabios.c:1267
    mov dx, ax                                ; 89 c2                       ; 0xc1b03
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b05
    mov bx, ax                                ; 89 c3                       ; 0xc1b08
    mov ax, dx                                ; 89 d0                       ; 0xc1b0a
    imul bx                                   ; f7 eb                       ; 0xc1b0c
    mov dl, cl                                ; 88 ca                       ; 0xc1b0e
    xor dh, dh                                ; 30 f6                       ; 0xc1b10
    imul dx                                   ; f7 ea                       ; 0xc1b12
    mov dx, ax                                ; 89 c2                       ; 0xc1b14
    sar dx, 1                                 ; d1 fa                       ; 0xc1b16
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1b18
    xor ah, ah                                ; 30 e4                       ; 0xc1b1b
    add dx, ax                                ; 01 c2                       ; 0xc1b1d
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1b1f
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1b22 vgabios.c:1268
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b25
    xor ah, ah                                ; 30 e4                       ; 0xc1b28
    cwd                                       ; 99                          ; 0xc1b2a
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1b2b
    sar ax, 1                                 ; d1 f8                       ; 0xc1b2d
    mov dx, ax                                ; 89 c2                       ; 0xc1b2f
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b31
    xor ah, ah                                ; 30 e4                       ; 0xc1b34
    cmp ax, dx                                ; 39 d0                       ; 0xc1b36
    jnl short 01b79h                          ; 7d 3f                       ; 0xc1b38
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1b3a vgabios.c:1270
    xor bh, bh                                ; 30 ff                       ; 0xc1b3d
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1b3f
    xor dh, dh                                ; 30 f6                       ; 0xc1b42
    mov si, dx                                ; 89 d6                       ; 0xc1b44
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1b46
    imul dx                                   ; f7 ea                       ; 0xc1b49
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1b4b
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1b4e
    add di, ax                                ; 01 c7                       ; 0xc1b51
    mov cx, bx                                ; 89 d9                       ; 0xc1b53
    mov ax, si                                ; 89 f0                       ; 0xc1b55
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1b57
    mov es, dx                                ; 8e c2                       ; 0xc1b5a
    jcxz 01b60h                               ; e3 02                       ; 0xc1b5c
    rep stosb                                 ; f3 aa                       ; 0xc1b5e
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1b60 vgabios.c:1271
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1b63
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc1b67
    mov cx, bx                                ; 89 d9                       ; 0xc1b6a
    mov ax, si                                ; 89 f0                       ; 0xc1b6c
    mov es, dx                                ; 8e c2                       ; 0xc1b6e
    jcxz 01b74h                               ; e3 02                       ; 0xc1b70
    rep stosb                                 ; f3 aa                       ; 0xc1b72
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1b74 vgabios.c:1272
    jmp short 01b25h                          ; eb ac                       ; 0xc1b77
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1b79 vgabios.c:1273
    pop di                                    ; 5f                          ; 0xc1b7c
    pop si                                    ; 5e                          ; 0xc1b7d
    pop bp                                    ; 5d                          ; 0xc1b7e
    retn 00004h                               ; c2 04 00                    ; 0xc1b7f
  ; disGetNextSymbol 0xc1b82 LB 0x2a8b -> off=0x0 cb=0000000000000083 uValue=00000000000c1b82 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1b82 LB 0x83
    push bp                                   ; 55                          ; 0xc1b82 vgabios.c:1276
    mov bp, sp                                ; 89 e5                       ; 0xc1b83
    push si                                   ; 56                          ; 0xc1b85
    push di                                   ; 57                          ; 0xc1b86
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1b87
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1b8a
    mov al, dl                                ; 88 d0                       ; 0xc1b8d
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1b8f
    mov bx, cx                                ; 89 cb                       ; 0xc1b92
    xor ah, ah                                ; 30 e4                       ; 0xc1b94 vgabios.c:1282
    mov si, ax                                ; 89 c6                       ; 0xc1b96
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1b98
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1b9b
    mov ax, si                                ; 89 f0                       ; 0xc1b9e
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc1ba0
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1ba3
    mov si, ax                                ; 89 c6                       ; 0xc1ba6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ba8
    xor ah, ah                                ; 30 e4                       ; 0xc1bab
    mov di, ax                                ; 89 c7                       ; 0xc1bad
    add si, ax                                ; 01 c6                       ; 0xc1baf
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1bb1
    sal si, CL                                ; d3 e6                       ; 0xc1bb3
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1bb5
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1bb8 vgabios.c:1283
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc1bbb
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1bbe
    add ax, di                                ; 01 f8                       ; 0xc1bc1
    sal ax, CL                                ; d3 e0                       ; 0xc1bc3
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1bc5
    sal bx, CL                                ; d3 e3                       ; 0xc1bc8 vgabios.c:1284
    sal word [bp+004h], CL                    ; d3 66 04                    ; 0xc1bca vgabios.c:1285
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1bcd vgabios.c:1286
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1bd1
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc1bd4
    jnc short 01bfch                          ; 73 23                       ; 0xc1bd7
    xor ah, ah                                ; 30 e4                       ; 0xc1bd9 vgabios.c:1288
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1bdb
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc1bde
    add si, ax                                ; 01 c6                       ; 0xc1be1
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1be3
    add di, ax                                ; 01 c7                       ; 0xc1be6
    mov cx, bx                                ; 89 d9                       ; 0xc1be8
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1bea
    mov es, dx                                ; 8e c2                       ; 0xc1bed
    jcxz 01bf7h                               ; e3 06                       ; 0xc1bef
    push DS                                   ; 1e                          ; 0xc1bf1
    mov ds, dx                                ; 8e da                       ; 0xc1bf2
    rep movsb                                 ; f3 a4                       ; 0xc1bf4
    pop DS                                    ; 1f                          ; 0xc1bf6
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1bf7 vgabios.c:1289
    jmp short 01bd1h                          ; eb d5                       ; 0xc1bfa
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1bfc vgabios.c:1290
    pop di                                    ; 5f                          ; 0xc1bff
    pop si                                    ; 5e                          ; 0xc1c00
    pop bp                                    ; 5d                          ; 0xc1c01
    retn 00004h                               ; c2 04 00                    ; 0xc1c02
  ; disGetNextSymbol 0xc1c05 LB 0x2a08 -> off=0x0 cb=000000000000006c uValue=00000000000c1c05 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc1c05 LB 0x6c
    push bp                                   ; 55                          ; 0xc1c05 vgabios.c:1293
    mov bp, sp                                ; 89 e5                       ; 0xc1c06
    push si                                   ; 56                          ; 0xc1c08
    push di                                   ; 57                          ; 0xc1c09
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1c0a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1c0d
    mov al, dl                                ; 88 d0                       ; 0xc1c10
    mov si, cx                                ; 89 ce                       ; 0xc1c12
    xor ah, ah                                ; 30 e4                       ; 0xc1c14 vgabios.c:1299
    mov dx, ax                                ; 89 c2                       ; 0xc1c16
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c18
    mov di, ax                                ; 89 c7                       ; 0xc1c1b
    mov ax, dx                                ; 89 d0                       ; 0xc1c1d
    imul di                                   ; f7 ef                       ; 0xc1c1f
    mul cx                                    ; f7 e1                       ; 0xc1c21
    mov dx, ax                                ; 89 c2                       ; 0xc1c23
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c25
    xor ah, ah                                ; 30 e4                       ; 0xc1c28
    add ax, dx                                ; 01 d0                       ; 0xc1c2a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1c2c
    sal ax, CL                                ; d3 e0                       ; 0xc1c2e
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1c30
    sal bx, CL                                ; d3 e3                       ; 0xc1c33 vgabios.c:1300
    sal si, CL                                ; d3 e6                       ; 0xc1c35 vgabios.c:1301
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1c37 vgabios.c:1302
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1c3b
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1c3e
    jnc short 01c68h                          ; 73 25                       ; 0xc1c41
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1c43 vgabios.c:1304
    xor ah, ah                                ; 30 e4                       ; 0xc1c46
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1c48
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1c4b
    mul si                                    ; f7 e6                       ; 0xc1c4e
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1c50
    add di, ax                                ; 01 c7                       ; 0xc1c53
    mov cx, bx                                ; 89 d9                       ; 0xc1c55
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1c57
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1c5a
    mov es, dx                                ; 8e c2                       ; 0xc1c5d
    jcxz 01c63h                               ; e3 02                       ; 0xc1c5f
    rep stosb                                 ; f3 aa                       ; 0xc1c61
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1c63 vgabios.c:1305
    jmp short 01c3bh                          ; eb d3                       ; 0xc1c66
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1c68 vgabios.c:1306
    pop di                                    ; 5f                          ; 0xc1c6b
    pop si                                    ; 5e                          ; 0xc1c6c
    pop bp                                    ; 5d                          ; 0xc1c6d
    retn 00004h                               ; c2 04 00                    ; 0xc1c6e
  ; disGetNextSymbol 0xc1c71 LB 0x299c -> off=0x0 cb=00000000000006a3 uValue=00000000000c1c71 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1c71 LB 0x6a3
    push bp                                   ; 55                          ; 0xc1c71 vgabios.c:1309
    mov bp, sp                                ; 89 e5                       ; 0xc1c72
    push si                                   ; 56                          ; 0xc1c74
    push di                                   ; 57                          ; 0xc1c75
    sub sp, strict byte 00020h                ; 83 ec 20                    ; 0xc1c76
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1c79
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1c7c
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1c7f
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1c82
    mov ch, byte [bp+006h]                    ; 8a 6e 06                    ; 0xc1c85
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1c88 vgabios.c:1318
    jnbe short 01ca8h                         ; 77 1b                       ; 0xc1c8b
    cmp ch, cl                                ; 38 cd                       ; 0xc1c8d vgabios.c:1319
    jc short 01ca8h                           ; 72 17                       ; 0xc1c8f
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1c91 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1c94
    mov es, ax                                ; 8e c0                       ; 0xc1c97
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1c99
    xor ah, ah                                ; 30 e4                       ; 0xc1c9c vgabios.c:1323
    call 03940h                               ; e8 9f 1c                    ; 0xc1c9e
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1ca1
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1ca4 vgabios.c:1324
    jne short 01cabh                          ; 75 03                       ; 0xc1ca6
    jmp near 0230bh                           ; e9 60 06                    ; 0xc1ca8
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1cab vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1cae
    mov es, ax                                ; 8e c0                       ; 0xc1cb1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1cb3
    xor ah, ah                                ; 30 e4                       ; 0xc1cb6 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc1cb8
    mov word [bp-024h], ax                    ; 89 46 dc                    ; 0xc1cb9
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1cbc vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1cbf
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1cc2 vgabios.c:58
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1cc5 vgabios.c:1331
    jne short 01cd4h                          ; 75 09                       ; 0xc1cc9
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1ccb vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1cce
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1cd1 vgabios.c:48
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1cd4 vgabios.c:1334
    xor ah, ah                                ; 30 e4                       ; 0xc1cd7
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1cd9
    jc short 01ce6h                           ; 72 08                       ; 0xc1cdc
    mov al, byte [bp-024h]                    ; 8a 46 dc                    ; 0xc1cde
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ce1
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1ce3
    mov al, ch                                ; 88 e8                       ; 0xc1ce6 vgabios.c:1335
    xor ah, ah                                ; 30 e4                       ; 0xc1ce8
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc1cea
    jc short 01cf4h                           ; 72 05                       ; 0xc1ced
    mov ch, byte [bp-018h]                    ; 8a 6e e8                    ; 0xc1cef
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc1cf2
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1cf4 vgabios.c:1336
    xor ah, ah                                ; 30 e4                       ; 0xc1cf7
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1cf9
    jbe short 01d01h                          ; 76 03                       ; 0xc1cfc
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1cfe
    mov al, ch                                ; 88 e8                       ; 0xc1d01 vgabios.c:1337
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1d03
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1d06
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1d08
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1d0b vgabios.c:1339
    mov byte [bp-01eh], al                    ; 88 46 e2                    ; 0xc1d0e
    mov byte [bp-01dh], 000h                  ; c6 46 e3 00                 ; 0xc1d11
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1d15
    mov bx, word [bp-01eh]                    ; 8b 5e e2                    ; 0xc1d17
    sal bx, CL                                ; d3 e3                       ; 0xc1d1a
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1d1c
    dec ax                                    ; 48                          ; 0xc1d1f
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1d20
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1d23
    dec ax                                    ; 48                          ; 0xc1d26
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1d27
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1d2a
    mul word [bp-024h]                        ; f7 66 dc                    ; 0xc1d2d
    mov di, ax                                ; 89 c7                       ; 0xc1d30
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1d32
    jne short 01d83h                          ; 75 4a                       ; 0xc1d37
    sal ax, 1                                 ; d1 e0                       ; 0xc1d39 vgabios.c:1342
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1d3b
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1d3d
    xor dh, dh                                ; 30 f6                       ; 0xc1d40
    inc ax                                    ; 40                          ; 0xc1d42
    mul dx                                    ; f7 e2                       ; 0xc1d43
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1d45
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d48 vgabios.c:1347
    jne short 01d86h                          ; 75 38                       ; 0xc1d4c
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d4e
    jne short 01d86h                          ; 75 32                       ; 0xc1d52
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d54
    jne short 01d86h                          ; 75 2c                       ; 0xc1d58
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d5a
    xor ah, ah                                ; 30 e4                       ; 0xc1d5d
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1d5f
    jne short 01d86h                          ; 75 22                       ; 0xc1d62
    mov al, ch                                ; 88 e8                       ; 0xc1d64
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc1d66
    jne short 01d86h                          ; 75 1b                       ; 0xc1d69
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1d6b vgabios.c:1349
    xor al, ch                                ; 30 e8                       ; 0xc1d6e
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1d70
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1d73
    mov cx, di                                ; 89 f9                       ; 0xc1d77
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1d79
    jcxz 01d80h                               ; e3 02                       ; 0xc1d7c
    rep stosw                                 ; f3 ab                       ; 0xc1d7e
    jmp near 0230bh                           ; e9 88 05                    ; 0xc1d80 vgabios.c:1351
    jmp near 01f10h                           ; e9 8a 01                    ; 0xc1d83
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d86 vgabios.c:1353
    jne short 01df1h                          ; 75 65                       ; 0xc1d8a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1d8c vgabios.c:1354
    xor ah, ah                                ; 30 e4                       ; 0xc1d8f
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1d91
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1d94
    xor dh, dh                                ; 30 f6                       ; 0xc1d97
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1d99
    jc short 01df3h                           ; 72 55                       ; 0xc1d9c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1d9e vgabios.c:1356
    xor ah, ah                                ; 30 e4                       ; 0xc1da1
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc1da3
    cmp ax, dx                                ; 39 d0                       ; 0xc1da6
    jnbe short 01db0h                         ; 77 06                       ; 0xc1da8
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1daa
    jne short 01df6h                          ; 75 46                       ; 0xc1dae
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1db0 vgabios.c:1357
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1db3
    xor al, al                                ; 30 c0                       ; 0xc1db6
    mov byte [bp-019h], al                    ; 88 46 e7                    ; 0xc1db8
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1dbb
    mov si, ax                                ; 89 c6                       ; 0xc1dbe
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1dc0
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1dc3
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1dc6
    mov dx, ax                                ; 89 c2                       ; 0xc1dc9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1dcb
    xor ah, ah                                ; 30 e4                       ; 0xc1dce
    add ax, dx                                ; 01 d0                       ; 0xc1dd0
    sal ax, 1                                 ; d1 e0                       ; 0xc1dd2
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1dd4
    add di, ax                                ; 01 c7                       ; 0xc1dd7
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1dd9
    xor bh, bh                                ; 30 ff                       ; 0xc1ddc
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1dde
    sal bx, CL                                ; d3 e3                       ; 0xc1de0
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1de2
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1de6
    mov ax, si                                ; 89 f0                       ; 0xc1de9
    jcxz 01defh                               ; e3 02                       ; 0xc1deb
    rep stosw                                 ; f3 ab                       ; 0xc1ded
    jmp short 01e3fh                          ; eb 4e                       ; 0xc1def vgabios.c:1358
    jmp short 01e45h                          ; eb 52                       ; 0xc1df1
    jmp near 0230bh                           ; e9 15 05                    ; 0xc1df3
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc1df6 vgabios.c:1359
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1df9
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc1dfc
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1dff
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1e02
    mov byte [bp-01ah], dl                    ; 88 56 e6                    ; 0xc1e05
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1e08
    mov si, ax                                ; 89 c6                       ; 0xc1e0c
    add si, word [bp-01ah]                    ; 03 76 e6                    ; 0xc1e0e
    sal si, 1                                 ; d1 e6                       ; 0xc1e11
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1e13
    xor bh, bh                                ; 30 ff                       ; 0xc1e16
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1e18
    sal bx, CL                                ; d3 e3                       ; 0xc1e1a
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1e1c
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1e20
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1e23
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1e26
    sal ax, 1                                 ; d1 e0                       ; 0xc1e29
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1e2b
    add di, ax                                ; 01 c7                       ; 0xc1e2e
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1e30
    mov dx, bx                                ; 89 da                       ; 0xc1e33
    mov es, bx                                ; 8e c3                       ; 0xc1e35
    jcxz 01e3fh                               ; e3 06                       ; 0xc1e37
    push DS                                   ; 1e                          ; 0xc1e39
    mov ds, dx                                ; 8e da                       ; 0xc1e3a
    rep movsw                                 ; f3 a5                       ; 0xc1e3c
    pop DS                                    ; 1f                          ; 0xc1e3e
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1e3f vgabios.c:1360
    jmp near 01d94h                           ; e9 4f ff                    ; 0xc1e42
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e45 vgabios.c:1363
    xor ah, ah                                ; 30 e4                       ; 0xc1e48
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1e4a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1e4d
    xor ah, ah                                ; 30 e4                       ; 0xc1e50
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e52
    jnbe short 01df3h                         ; 77 9c                       ; 0xc1e55
    mov dl, al                                ; 88 c2                       ; 0xc1e57 vgabios.c:1365
    xor dh, dh                                ; 30 f6                       ; 0xc1e59
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1e5b
    add ax, dx                                ; 01 d0                       ; 0xc1e5e
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e60
    jnbe short 01e6bh                         ; 77 06                       ; 0xc1e63
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1e65
    jne short 01eabh                          ; 75 40                       ; 0xc1e69
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e6b vgabios.c:1366
    xor bh, bh                                ; 30 ff                       ; 0xc1e6e
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1e70
    xor al, al                                ; 30 c0                       ; 0xc1e73
    mov si, ax                                ; 89 c6                       ; 0xc1e75
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1e77
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1e7a
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1e7d
    mov dx, ax                                ; 89 c2                       ; 0xc1e80
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e82
    xor ah, ah                                ; 30 e4                       ; 0xc1e85
    add ax, dx                                ; 01 d0                       ; 0xc1e87
    sal ax, 1                                 ; d1 e0                       ; 0xc1e89
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1e8b
    add dx, ax                                ; 01 c2                       ; 0xc1e8e
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1e90
    xor ah, ah                                ; 30 e4                       ; 0xc1e93
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1e95
    mov di, ax                                ; 89 c7                       ; 0xc1e97
    sal di, CL                                ; d3 e7                       ; 0xc1e99
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc1e9b
    mov cx, bx                                ; 89 d9                       ; 0xc1e9f
    mov ax, si                                ; 89 f0                       ; 0xc1ea1
    mov di, dx                                ; 89 d7                       ; 0xc1ea3
    jcxz 01ea9h                               ; e3 02                       ; 0xc1ea5
    rep stosw                                 ; f3 ab                       ; 0xc1ea7
    jmp short 01f00h                          ; eb 55                       ; 0xc1ea9 vgabios.c:1367
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1eab vgabios.c:1368
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1eae
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc1eb1
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1eb4
    xor ah, ah                                ; 30 e4                       ; 0xc1eb7
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1eb9
    sub dx, ax                                ; 29 c2                       ; 0xc1ebc
    mov ax, dx                                ; 89 d0                       ; 0xc1ebe
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1ec0
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1ec3
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1ec6
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc1ec9
    mov si, ax                                ; 89 c6                       ; 0xc1ecd
    add si, word [bp-014h]                    ; 03 76 ec                    ; 0xc1ecf
    sal si, 1                                 ; d1 e6                       ; 0xc1ed2
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1ed4
    xor bh, bh                                ; 30 ff                       ; 0xc1ed7
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1ed9
    sal bx, CL                                ; d3 e3                       ; 0xc1edb
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1edd
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1ee1
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1ee4
    add ax, word [bp-014h]                    ; 03 46 ec                    ; 0xc1ee7
    sal ax, 1                                 ; d1 e0                       ; 0xc1eea
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1eec
    add di, ax                                ; 01 c7                       ; 0xc1eef
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1ef1
    mov dx, bx                                ; 89 da                       ; 0xc1ef4
    mov es, bx                                ; 8e c3                       ; 0xc1ef6
    jcxz 01f00h                               ; e3 06                       ; 0xc1ef8
    push DS                                   ; 1e                          ; 0xc1efa
    mov ds, dx                                ; 8e da                       ; 0xc1efb
    rep movsw                                 ; f3 a5                       ; 0xc1efd
    pop DS                                    ; 1f                          ; 0xc1eff
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f00 vgabios.c:1369
    xor ah, ah                                ; 30 e4                       ; 0xc1f03
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f05
    jc short 01f3eh                           ; 72 34                       ; 0xc1f08
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1f0a vgabios.c:1370
    jmp near 01e4dh                           ; e9 3d ff                    ; 0xc1f0d
    mov si, word [bp-01eh]                    ; 8b 76 e2                    ; 0xc1f10 vgabios.c:1376
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1f13
    xor ah, ah                                ; 30 e4                       ; 0xc1f17
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1f19
    mov si, ax                                ; 89 c6                       ; 0xc1f1b
    sal si, CL                                ; d3 e6                       ; 0xc1f1d
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1f1f
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1f23
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1f26 vgabios.c:1377
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1f2a
    jc short 01f3ah                           ; 72 0c                       ; 0xc1f2c
    jbe short 01f41h                          ; 76 11                       ; 0xc1f2e
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1f30
    je short 01f6eh                           ; 74 3a                       ; 0xc1f32
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1f34
    je short 01f41h                           ; 74 09                       ; 0xc1f36
    jmp short 01f3eh                          ; eb 04                       ; 0xc1f38
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1f3a
    je short 01f71h                           ; 74 33                       ; 0xc1f3c
    jmp near 0230bh                           ; e9 ca 03                    ; 0xc1f3e
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f41 vgabios.c:1381
    jne short 01f6ch                          ; 75 25                       ; 0xc1f45
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1f47
    jne short 01fafh                          ; 75 62                       ; 0xc1f4b
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f4d
    jne short 01fafh                          ; 75 5c                       ; 0xc1f51
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f53
    xor ah, ah                                ; 30 e4                       ; 0xc1f56
    mov dx, word [bp-024h]                    ; 8b 56 dc                    ; 0xc1f58
    dec dx                                    ; 4a                          ; 0xc1f5b
    cmp ax, dx                                ; 39 d0                       ; 0xc1f5c
    jne short 01fafh                          ; 75 4f                       ; 0xc1f5e
    mov al, ch                                ; 88 e8                       ; 0xc1f60
    xor ah, dh                                ; 30 f4                       ; 0xc1f62
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1f64
    dec dx                                    ; 4a                          ; 0xc1f67
    cmp ax, dx                                ; 39 d0                       ; 0xc1f68
    je short 01f74h                           ; 74 08                       ; 0xc1f6a
    jmp short 01fafh                          ; eb 41                       ; 0xc1f6c
    jmp near 021efh                           ; e9 7e 02                    ; 0xc1f6e
    jmp near 0209bh                           ; e9 27 01                    ; 0xc1f71
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1f74 vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1f77
    out DX, ax                                ; ef                          ; 0xc1f7a
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1f7b vgabios.c:1384
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1f7e
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1f81
    xor dh, dh                                ; 30 f6                       ; 0xc1f84
    mul dx                                    ; f7 e2                       ; 0xc1f86
    mov dx, ax                                ; 89 c2                       ; 0xc1f88
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f8a
    xor ah, ah                                ; 30 e4                       ; 0xc1f8d
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1f8f
    xor bh, bh                                ; 30 ff                       ; 0xc1f92
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1f94
    sal bx, CL                                ; d3 e3                       ; 0xc1f96
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1f98
    mov cx, dx                                ; 89 d1                       ; 0xc1f9c
    xor di, di                                ; 31 ff                       ; 0xc1f9e
    mov es, bx                                ; 8e c3                       ; 0xc1fa0
    jcxz 01fa6h                               ; e3 02                       ; 0xc1fa2
    rep stosb                                 ; f3 aa                       ; 0xc1fa4
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1fa6 vgabios.c:1385
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1fa9
    out DX, ax                                ; ef                          ; 0xc1fac
    jmp short 01f3eh                          ; eb 8f                       ; 0xc1fad vgabios.c:1387
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1faf vgabios.c:1389
    jne short 02021h                          ; 75 6c                       ; 0xc1fb3
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1fb5 vgabios.c:1390
    xor ah, ah                                ; 30 e4                       ; 0xc1fb8
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1fba
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1fbd
    xor ah, ah                                ; 30 e4                       ; 0xc1fc0
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1fc2
    jc short 0201eh                           ; 72 57                       ; 0xc1fc5
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1fc7 vgabios.c:1392
    xor dh, dh                                ; 30 f6                       ; 0xc1fca
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc1fcc
    cmp dx, ax                                ; 39 c2                       ; 0xc1fcf
    jnbe short 01fd9h                         ; 77 06                       ; 0xc1fd1
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1fd3
    jne short 01ffah                          ; 75 21                       ; 0xc1fd7
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1fd9 vgabios.c:1393
    xor ah, ah                                ; 30 e4                       ; 0xc1fdc
    push ax                                   ; 50                          ; 0xc1fde
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1fdf
    push ax                                   ; 50                          ; 0xc1fe2
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1fe3
    xor ch, ch                                ; 30 ed                       ; 0xc1fe6
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1fe8
    xor bh, bh                                ; 30 ff                       ; 0xc1feb
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1fed
    xor dh, dh                                ; 30 f6                       ; 0xc1ff0
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ff2
    call 019bdh                               ; e8 c5 f9                    ; 0xc1ff5
    jmp short 02019h                          ; eb 1f                       ; 0xc1ff8 vgabios.c:1394
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1ffa vgabios.c:1395
    push ax                                   ; 50                          ; 0xc1ffd
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1ffe
    push ax                                   ; 50                          ; 0xc2001
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2002
    xor ch, ch                                ; 30 ed                       ; 0xc2005
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2007
    xor bh, bh                                ; 30 ff                       ; 0xc200a
    mov dl, bl                                ; 88 da                       ; 0xc200c
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc200e
    xor dh, dh                                ; 30 f6                       ; 0xc2011
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2013
    call 0192fh                               ; e8 16 f9                    ; 0xc2016
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc2019 vgabios.c:1396
    jmp short 01fbdh                          ; eb 9f                       ; 0xc201c
    jmp near 0230bh                           ; e9 ea 02                    ; 0xc201e
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2021 vgabios.c:1399
    xor ah, ah                                ; 30 e4                       ; 0xc2024
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2026
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2029
    xor ah, ah                                ; 30 e4                       ; 0xc202c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc202e
    jnbe short 0201eh                         ; 77 eb                       ; 0xc2031
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2033 vgabios.c:1401
    xor dh, dh                                ; 30 f6                       ; 0xc2036
    add ax, dx                                ; 01 d0                       ; 0xc2038
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc203a
    jnbe short 02043h                         ; 77 04                       ; 0xc203d
    test dl, dl                               ; 84 d2                       ; 0xc203f
    jne short 02064h                          ; 75 21                       ; 0xc2041
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2043 vgabios.c:1402
    xor ah, ah                                ; 30 e4                       ; 0xc2046
    push ax                                   ; 50                          ; 0xc2048
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2049
    push ax                                   ; 50                          ; 0xc204c
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc204d
    xor ch, ch                                ; 30 ed                       ; 0xc2050
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2052
    xor bh, bh                                ; 30 ff                       ; 0xc2055
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc2057
    xor dh, dh                                ; 30 f6                       ; 0xc205a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc205c
    call 019bdh                               ; e8 5b f9                    ; 0xc205f
    jmp short 0208ch                          ; eb 28                       ; 0xc2062 vgabios.c:1403
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2064 vgabios.c:1404
    xor ah, ah                                ; 30 e4                       ; 0xc2067
    push ax                                   ; 50                          ; 0xc2069
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc206a
    push ax                                   ; 50                          ; 0xc206d
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc206e
    xor ch, ch                                ; 30 ed                       ; 0xc2071
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2073
    xor bh, bh                                ; 30 ff                       ; 0xc2076
    mov dl, bl                                ; 88 da                       ; 0xc2078
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc207a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc207d
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc2080
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc2083
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2086
    call 0192fh                               ; e8 a3 f8                    ; 0xc2089
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc208c vgabios.c:1405
    xor ah, ah                                ; 30 e4                       ; 0xc208f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2091
    jc short 020e4h                           ; 72 4e                       ; 0xc2094
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc2096 vgabios.c:1406
    jmp short 02029h                          ; eb 8e                       ; 0xc2099
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc209b vgabios.c:1411
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc209f vgabios.c:1412
    jne short 020e7h                          ; 75 42                       ; 0xc20a3
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc20a5
    jne short 020e7h                          ; 75 3c                       ; 0xc20a9
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc20ab
    jne short 020e7h                          ; 75 36                       ; 0xc20af
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20b1
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc20b4
    jne short 020e7h                          ; 75 2e                       ; 0xc20b7
    mov al, ch                                ; 88 e8                       ; 0xc20b9
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc20bb
    jne short 020e7h                          ; 75 27                       ; 0xc20be
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc20c0 vgabios.c:1414
    xor dh, dh                                ; 30 f6                       ; 0xc20c3
    mov ax, di                                ; 89 f8                       ; 0xc20c5
    mul dx                                    ; f7 e2                       ; 0xc20c7
    mov dl, cl                                ; 88 ca                       ; 0xc20c9
    xor dh, dh                                ; 30 f6                       ; 0xc20cb
    mul dx                                    ; f7 e2                       ; 0xc20cd
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc20cf
    xor dh, dh                                ; 30 f6                       ; 0xc20d2
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc20d4
    mov cx, ax                                ; 89 c1                       ; 0xc20d8
    mov ax, dx                                ; 89 d0                       ; 0xc20da
    xor di, di                                ; 31 ff                       ; 0xc20dc
    mov es, bx                                ; 8e c3                       ; 0xc20de
    jcxz 020e4h                               ; e3 02                       ; 0xc20e0
    rep stosb                                 ; f3 aa                       ; 0xc20e2
    jmp near 0230bh                           ; e9 24 02                    ; 0xc20e4 vgabios.c:1416
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc20e7 vgabios.c:1418
    jne short 020f5h                          ; 75 09                       ; 0xc20ea
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc20ec vgabios.c:1420
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc20ef vgabios.c:1421
    sal word [bp-018h], 1                     ; d1 66 e8                    ; 0xc20f2 vgabios.c:1422
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc20f5 vgabios.c:1425
    jne short 02164h                          ; 75 69                       ; 0xc20f9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc20fb vgabios.c:1426
    xor ah, ah                                ; 30 e4                       ; 0xc20fe
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2100
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2103
    xor ah, ah                                ; 30 e4                       ; 0xc2106
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2108
    jc short 020e4h                           ; 72 d7                       ; 0xc210b
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc210d vgabios.c:1428
    xor dh, dh                                ; 30 f6                       ; 0xc2110
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc2112
    cmp dx, ax                                ; 39 c2                       ; 0xc2115
    jnbe short 0211fh                         ; 77 06                       ; 0xc2117
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2119
    jne short 02140h                          ; 75 21                       ; 0xc211d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc211f vgabios.c:1429
    xor ah, ah                                ; 30 e4                       ; 0xc2122
    push ax                                   ; 50                          ; 0xc2124
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2125
    push ax                                   ; 50                          ; 0xc2128
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc2129
    xor ch, ch                                ; 30 ed                       ; 0xc212c
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc212e
    xor bh, bh                                ; 30 ff                       ; 0xc2131
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc2133
    xor dh, dh                                ; 30 f6                       ; 0xc2136
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2138
    call 01aeeh                               ; e8 b0 f9                    ; 0xc213b
    jmp short 0215fh                          ; eb 1f                       ; 0xc213e vgabios.c:1430
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2140 vgabios.c:1431
    push ax                                   ; 50                          ; 0xc2143
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2144
    push ax                                   ; 50                          ; 0xc2147
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2148
    xor ch, ch                                ; 30 ed                       ; 0xc214b
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc214d
    xor bh, bh                                ; 30 ff                       ; 0xc2150
    mov dl, bl                                ; 88 da                       ; 0xc2152
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc2154
    xor dh, dh                                ; 30 f6                       ; 0xc2157
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2159
    call 01a38h                               ; e8 d9 f8                    ; 0xc215c
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc215f vgabios.c:1432
    jmp short 02103h                          ; eb 9f                       ; 0xc2162
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2164 vgabios.c:1435
    xor ah, ah                                ; 30 e4                       ; 0xc2167
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2169
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc216c
    xor ah, ah                                ; 30 e4                       ; 0xc216f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2171
    jnbe short 021b4h                         ; 77 3e                       ; 0xc2174
    mov dl, al                                ; 88 c2                       ; 0xc2176 vgabios.c:1437
    xor dh, dh                                ; 30 f6                       ; 0xc2178
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc217a
    add ax, dx                                ; 01 d0                       ; 0xc217d
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc217f
    jnbe short 0218ah                         ; 77 06                       ; 0xc2182
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2184
    jne short 021b7h                          ; 75 2d                       ; 0xc2188
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc218a vgabios.c:1438
    xor ah, ah                                ; 30 e4                       ; 0xc218d
    push ax                                   ; 50                          ; 0xc218f
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2190
    push ax                                   ; 50                          ; 0xc2193
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc2194
    xor ch, ch                                ; 30 ed                       ; 0xc2197
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2199
    xor bh, bh                                ; 30 ff                       ; 0xc219c
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc219e
    xor dh, dh                                ; 30 f6                       ; 0xc21a1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc21a3
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc21a6
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc21a9
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc21ac
    call 01aeeh                               ; e8 3c f9                    ; 0xc21af
    jmp short 021dfh                          ; eb 2b                       ; 0xc21b2 vgabios.c:1439
    jmp near 0230bh                           ; e9 54 01                    ; 0xc21b4
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc21b7 vgabios.c:1440
    xor ah, ah                                ; 30 e4                       ; 0xc21ba
    push ax                                   ; 50                          ; 0xc21bc
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc21bd
    push ax                                   ; 50                          ; 0xc21c0
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc21c1
    xor ch, ch                                ; 30 ed                       ; 0xc21c4
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc21c6
    xor bh, bh                                ; 30 ff                       ; 0xc21c9
    mov dl, bl                                ; 88 da                       ; 0xc21cb
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc21cd
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc21d0
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc21d3
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc21d6
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc21d9
    call 01a38h                               ; e8 59 f8                    ; 0xc21dc
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc21df vgabios.c:1441
    xor ah, ah                                ; 30 e4                       ; 0xc21e2
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc21e4
    jc short 0222eh                           ; 72 45                       ; 0xc21e7
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc21e9 vgabios.c:1442
    jmp near 0216ch                           ; e9 7d ff                    ; 0xc21ec
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc21ef vgabios.c:1447
    jne short 02231h                          ; 75 3c                       ; 0xc21f3
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc21f5
    jne short 02231h                          ; 75 36                       ; 0xc21f9
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc21fb
    jne short 02231h                          ; 75 30                       ; 0xc21ff
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2201
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc2204
    jne short 02231h                          ; 75 28                       ; 0xc2207
    mov al, ch                                ; 88 e8                       ; 0xc2209
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc220b
    jne short 02231h                          ; 75 21                       ; 0xc220e
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2210 vgabios.c:1449
    xor dh, dh                                ; 30 f6                       ; 0xc2213
    mov ax, di                                ; 89 f8                       ; 0xc2215
    mul dx                                    ; f7 e2                       ; 0xc2217
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2219
    sal ax, CL                                ; d3 e0                       ; 0xc221b
    mov cx, ax                                ; 89 c1                       ; 0xc221d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc221f
    xor ah, ah                                ; 30 e4                       ; 0xc2222
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2224
    xor di, di                                ; 31 ff                       ; 0xc2228
    jcxz 0222eh                               ; e3 02                       ; 0xc222a
    rep stosb                                 ; f3 aa                       ; 0xc222c
    jmp near 0230bh                           ; e9 da 00                    ; 0xc222e vgabios.c:1451
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2231 vgabios.c:1454
    jne short 0229dh                          ; 75 66                       ; 0xc2235
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2237 vgabios.c:1455
    xor ah, ah                                ; 30 e4                       ; 0xc223a
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc223c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc223f
    xor ah, ah                                ; 30 e4                       ; 0xc2242
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2244
    jc short 0222eh                           ; 72 e5                       ; 0xc2247
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2249 vgabios.c:1457
    xor dh, dh                                ; 30 f6                       ; 0xc224c
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc224e
    cmp dx, ax                                ; 39 c2                       ; 0xc2251
    jnbe short 0225bh                         ; 77 06                       ; 0xc2253
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2255
    jne short 0227ah                          ; 75 1f                       ; 0xc2259
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc225b vgabios.c:1458
    xor ah, ah                                ; 30 e4                       ; 0xc225e
    push ax                                   ; 50                          ; 0xc2260
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2261
    push ax                                   ; 50                          ; 0xc2264
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2265
    xor bh, bh                                ; 30 ff                       ; 0xc2268
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc226a
    xor dh, dh                                ; 30 f6                       ; 0xc226d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc226f
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc2272
    call 01c05h                               ; e8 8d f9                    ; 0xc2275
    jmp short 02298h                          ; eb 1e                       ; 0xc2278 vgabios.c:1459
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc227a vgabios.c:1460
    push ax                                   ; 50                          ; 0xc227d
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc227e
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2281
    xor ch, ch                                ; 30 ed                       ; 0xc2284
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2286
    xor bh, bh                                ; 30 ff                       ; 0xc2289
    mov dl, bl                                ; 88 da                       ; 0xc228b
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc228d
    xor dh, dh                                ; 30 f6                       ; 0xc2290
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2292
    call 01b82h                               ; e8 ea f8                    ; 0xc2295
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc2298 vgabios.c:1461
    jmp short 0223fh                          ; eb a2                       ; 0xc229b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc229d vgabios.c:1464
    xor ah, ah                                ; 30 e4                       ; 0xc22a0
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc22a2
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc22a5
    xor ah, ah                                ; 30 e4                       ; 0xc22a8
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc22aa
    jnbe short 0230bh                         ; 77 5c                       ; 0xc22ad
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc22af vgabios.c:1466
    xor dh, dh                                ; 30 f6                       ; 0xc22b2
    add ax, dx                                ; 01 d0                       ; 0xc22b4
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc22b6
    jnbe short 022bfh                         ; 77 04                       ; 0xc22b9
    test dl, dl                               ; 84 d2                       ; 0xc22bb
    jne short 022deh                          ; 75 1f                       ; 0xc22bd
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc22bf vgabios.c:1467
    xor ah, ah                                ; 30 e4                       ; 0xc22c2
    push ax                                   ; 50                          ; 0xc22c4
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc22c5
    push ax                                   ; 50                          ; 0xc22c8
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc22c9
    xor bh, bh                                ; 30 ff                       ; 0xc22cc
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc22ce
    xor dh, dh                                ; 30 f6                       ; 0xc22d1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc22d3
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc22d6
    call 01c05h                               ; e8 29 f9                    ; 0xc22d9
    jmp short 022fch                          ; eb 1e                       ; 0xc22dc vgabios.c:1468
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc22de vgabios.c:1469
    xor ah, ah                                ; 30 e4                       ; 0xc22e1
    push ax                                   ; 50                          ; 0xc22e3
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc22e4
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc22e7
    xor ch, ch                                ; 30 ed                       ; 0xc22ea
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc22ec
    xor bh, bh                                ; 30 ff                       ; 0xc22ef
    mov dl, bl                                ; 88 da                       ; 0xc22f1
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc22f3
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc22f6
    call 01b82h                               ; e8 86 f8                    ; 0xc22f9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc22fc vgabios.c:1470
    xor ah, ah                                ; 30 e4                       ; 0xc22ff
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2301
    jc short 0230bh                           ; 72 05                       ; 0xc2304
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc2306 vgabios.c:1471
    jmp short 022a5h                          ; eb 9a                       ; 0xc2309
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc230b vgabios.c:1482
    pop di                                    ; 5f                          ; 0xc230e
    pop si                                    ; 5e                          ; 0xc230f
    pop bp                                    ; 5d                          ; 0xc2310
    retn 00008h                               ; c2 08 00                    ; 0xc2311
  ; disGetNextSymbol 0xc2314 LB 0x22f9 -> off=0x0 cb=0000000000000112 uValue=00000000000c2314 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc2314 LB 0x112
    push bp                                   ; 55                          ; 0xc2314 vgabios.c:1485
    mov bp, sp                                ; 89 e5                       ; 0xc2315
    push si                                   ; 56                          ; 0xc2317
    push di                                   ; 57                          ; 0xc2318
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc2319
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc231c
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc231f
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2322
    mov al, cl                                ; 88 c8                       ; 0xc2325
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc2327 vgabios.c:67
    xor cx, cx                                ; 31 c9                       ; 0xc232a
    mov es, cx                                ; 8e c1                       ; 0xc232c
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc232e
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc2331
    mov word [bp-014h], cx                    ; 89 4e ec                    ; 0xc2335 vgabios.c:68
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc2338
    xor ah, ah                                ; 30 e4                       ; 0xc233b vgabios.c:1494
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc233d
    xor ch, ch                                ; 30 ed                       ; 0xc2340
    imul cx                                   ; f7 e9                       ; 0xc2342
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2344
    xor bh, bh                                ; 30 ff                       ; 0xc2347
    imul bx                                   ; f7 eb                       ; 0xc2349
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc234b
    mov si, bx                                ; 89 de                       ; 0xc234e
    add si, ax                                ; 01 c6                       ; 0xc2350
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2352 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2355
    mov es, ax                                ; 8e c0                       ; 0xc2358
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc235a
    mov bl, byte [bp+008h]                    ; 8a 5e 08                    ; 0xc235d vgabios.c:58
    xor bh, bh                                ; 30 ff                       ; 0xc2360
    mul bx                                    ; f7 e3                       ; 0xc2362
    add si, ax                                ; 01 c6                       ; 0xc2364
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2366 vgabios.c:1496
    xor ah, ah                                ; 30 e4                       ; 0xc2369
    imul cx                                   ; f7 e9                       ; 0xc236b
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc236d
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc2370 vgabios.c:1497
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2373
    out DX, ax                                ; ef                          ; 0xc2376
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2377 vgabios.c:1498
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc237a
    out DX, ax                                ; ef                          ; 0xc237d
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc237e vgabios.c:1499
    je short 0238ah                           ; 74 06                       ; 0xc2382
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2384 vgabios.c:1501
    out DX, ax                                ; ef                          ; 0xc2387
    jmp short 0238eh                          ; eb 04                       ; 0xc2388 vgabios.c:1503
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc238a vgabios.c:1505
    out DX, ax                                ; ef                          ; 0xc238d
    xor ch, ch                                ; 30 ed                       ; 0xc238e vgabios.c:1507
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc2390
    jnc short 023aah                          ; 73 15                       ; 0xc2393
    mov al, ch                                ; 88 e8                       ; 0xc2395 vgabios.c:1509
    xor ah, ah                                ; 30 e4                       ; 0xc2397
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2399
    xor bh, bh                                ; 30 ff                       ; 0xc239c
    imul bx                                   ; f7 eb                       ; 0xc239e
    mov bx, si                                ; 89 f3                       ; 0xc23a0
    add bx, ax                                ; 01 c3                       ; 0xc23a2
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc23a4 vgabios.c:1510
    jmp short 023beh                          ; eb 14                       ; 0xc23a8
    jmp short 0240eh                          ; eb 62                       ; 0xc23aa vgabios.c:1519
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc23ac vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc23af
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc23b1
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc23b5 vgabios.c:1523
    cmp byte [bp-008h], 008h                  ; 80 7e f8 08                 ; 0xc23b8
    jnc short 0240ah                          ; 73 4c                       ; 0xc23bc
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc23be
    mov ax, 00080h                            ; b8 80 00                    ; 0xc23c1
    sar ax, CL                                ; d3 f8                       ; 0xc23c4
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc23c6
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc23c9
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc23cd
    mov ah, al                                ; 88 c4                       ; 0xc23d0
    xor al, al                                ; 30 c0                       ; 0xc23d2
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc23d4
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc23d6
    out DX, ax                                ; ef                          ; 0xc23d9
    mov dx, bx                                ; 89 da                       ; 0xc23da
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc23dc
    call 0396bh                               ; e8 89 15                    ; 0xc23df
    mov al, ch                                ; 88 e8                       ; 0xc23e2
    xor ah, ah                                ; 30 e4                       ; 0xc23e4
    add ax, word [bp-012h]                    ; 03 46 ee                    ; 0xc23e6
    mov es, [bp-010h]                         ; 8e 46 f0                    ; 0xc23e9
    mov di, word [bp-014h]                    ; 8b 7e ec                    ; 0xc23ec
    add di, ax                                ; 01 c7                       ; 0xc23ef
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc23f1
    xor ah, ah                                ; 30 e4                       ; 0xc23f4
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc23f6
    je short 023ach                           ; 74 b1                       ; 0xc23f9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc23fb
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc23fe
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2400
    mov es, di                                ; 8e c7                       ; 0xc2403
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2405
    jmp short 023b5h                          ; eb ab                       ; 0xc2408
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc240a vgabios.c:1524
    jmp short 02390h                          ; eb 82                       ; 0xc240c
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc240e vgabios.c:1525
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2411
    out DX, ax                                ; ef                          ; 0xc2414
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2415 vgabios.c:1526
    out DX, ax                                ; ef                          ; 0xc2418
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2419 vgabios.c:1527
    out DX, ax                                ; ef                          ; 0xc241c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc241d vgabios.c:1528
    pop di                                    ; 5f                          ; 0xc2420
    pop si                                    ; 5e                          ; 0xc2421
    pop bp                                    ; 5d                          ; 0xc2422
    retn 00006h                               ; c2 06 00                    ; 0xc2423
  ; disGetNextSymbol 0xc2426 LB 0x21e7 -> off=0x0 cb=0000000000000112 uValue=00000000000c2426 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc2426 LB 0x112
    push si                                   ; 56                          ; 0xc2426 vgabios.c:1531
    push di                                   ; 57                          ; 0xc2427
    push bp                                   ; 55                          ; 0xc2428
    mov bp, sp                                ; 89 e5                       ; 0xc2429
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc242b
    mov ch, al                                ; 88 c5                       ; 0xc242e
    mov byte [bp-002h], dl                    ; 88 56 fe                    ; 0xc2430
    mov al, bl                                ; 88 d8                       ; 0xc2433
    mov si, 0556ch                            ; be 6c 55                    ; 0xc2435 vgabios.c:1538
    xor ah, ah                                ; 30 e4                       ; 0xc2438 vgabios.c:1539
    mov bl, byte [bp+00ah]                    ; 8a 5e 0a                    ; 0xc243a
    xor bh, bh                                ; 30 ff                       ; 0xc243d
    imul bx                                   ; f7 eb                       ; 0xc243f
    mov bx, ax                                ; 89 c3                       ; 0xc2441
    mov al, cl                                ; 88 c8                       ; 0xc2443
    xor ah, ah                                ; 30 e4                       ; 0xc2445
    mov di, 00140h                            ; bf 40 01                    ; 0xc2447
    imul di                                   ; f7 ef                       ; 0xc244a
    add bx, ax                                ; 01 c3                       ; 0xc244c
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc244e
    mov al, ch                                ; 88 e8                       ; 0xc2451 vgabios.c:1540
    xor ah, ah                                ; 30 e4                       ; 0xc2453
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2455
    sal ax, CL                                ; d3 e0                       ; 0xc2457
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2459
    xor ch, ch                                ; 30 ed                       ; 0xc245c vgabios.c:1541
    jmp near 0247dh                           ; e9 1c 00                    ; 0xc245e
    mov al, ch                                ; 88 e8                       ; 0xc2461 vgabios.c:1556
    xor ah, ah                                ; 30 e4                       ; 0xc2463
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc2465
    mov di, si                                ; 89 f7                       ; 0xc2468
    add di, ax                                ; 01 c7                       ; 0xc246a
    mov al, byte [di]                         ; 8a 05                       ; 0xc246c
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc246e vgabios.c:52
    mov es, di                                ; 8e c7                       ; 0xc2471
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2473
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2476 vgabios.c:1560
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2478
    jnc short 024d5h                          ; 73 58                       ; 0xc247b
    mov al, ch                                ; 88 e8                       ; 0xc247d
    xor ah, ah                                ; 30 e4                       ; 0xc247f
    sar ax, 1                                 ; d1 f8                       ; 0xc2481
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc2483
    imul bx                                   ; f7 eb                       ; 0xc2486
    mov bx, word [bp-004h]                    ; 8b 5e fc                    ; 0xc2488
    add bx, ax                                ; 01 c3                       ; 0xc248b
    test ch, 001h                             ; f6 c5 01                    ; 0xc248d
    je short 02495h                           ; 74 03                       ; 0xc2490
    add bh, 020h                              ; 80 c7 20                    ; 0xc2492
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc2495
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2497
    jne short 024bbh                          ; 75 1e                       ; 0xc249b
    test byte [bp-002h], dl                   ; 84 56 fe                    ; 0xc249d
    je short 02461h                           ; 74 bf                       ; 0xc24a0
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc24a2
    mov es, ax                                ; 8e c0                       ; 0xc24a5
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc24a7
    mov al, ch                                ; 88 e8                       ; 0xc24aa
    xor ah, ah                                ; 30 e4                       ; 0xc24ac
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc24ae
    mov di, si                                ; 89 f7                       ; 0xc24b1
    add di, ax                                ; 01 c7                       ; 0xc24b3
    mov al, byte [di]                         ; 8a 05                       ; 0xc24b5
    xor al, dl                                ; 30 d0                       ; 0xc24b7
    jmp short 0246eh                          ; eb b3                       ; 0xc24b9
    test dl, dl                               ; 84 d2                       ; 0xc24bb vgabios.c:1562
    jbe short 02476h                          ; 76 b7                       ; 0xc24bd
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc24bf vgabios.c:1564
    je short 024cfh                           ; 74 0a                       ; 0xc24c3
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc24c5 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc24c8
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc24ca
    jmp short 024d1h                          ; eb 02                       ; 0xc24cd vgabios.c:1568
    xor al, al                                ; 30 c0                       ; 0xc24cf vgabios.c:1570
    xor ah, ah                                ; 30 e4                       ; 0xc24d1 vgabios.c:1572
    jmp short 024dch                          ; eb 07                       ; 0xc24d3
    jmp short 02530h                          ; eb 59                       ; 0xc24d5
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc24d7
    jnc short 02525h                          ; 73 49                       ; 0xc24da
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc24dc vgabios.c:1574
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc24df
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc24e3
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc24e6
    add di, si                                ; 01 f7                       ; 0xc24e9
    mov cl, byte [di]                         ; 8a 0d                       ; 0xc24eb
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc24ed
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc24f0
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc24f4
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc24f7
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc24fb
    test word [bp-006h], di                   ; 85 7e fa                    ; 0xc24fe
    je short 0251fh                           ; 74 1c                       ; 0xc2501
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2503 vgabios.c:1575
    sub cl, ah                                ; 28 e1                       ; 0xc2505
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc2507
    and dh, 003h                              ; 80 e6 03                    ; 0xc250a
    sal cl, 1                                 ; d0 e1                       ; 0xc250d
    sal dh, CL                                ; d2 e6                       ; 0xc250f
    mov cl, dh                                ; 88 f1                       ; 0xc2511
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc2513 vgabios.c:1576
    je short 0251dh                           ; 74 04                       ; 0xc2517
    xor al, dh                                ; 30 f0                       ; 0xc2519 vgabios.c:1578
    jmp short 0251fh                          ; eb 02                       ; 0xc251b vgabios.c:1580
    or al, dh                                 ; 08 f0                       ; 0xc251d vgabios.c:1582
    shr dl, 1                                 ; d0 ea                       ; 0xc251f vgabios.c:1585
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2521 vgabios.c:1586
    jmp short 024d7h                          ; eb b2                       ; 0xc2523
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc2525 vgabios.c:52
    mov es, di                                ; 8e c7                       ; 0xc2528
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc252a
    inc bx                                    ; 43                          ; 0xc252d vgabios.c:1588
    jmp short 024bbh                          ; eb 8b                       ; 0xc252e vgabios.c:1589
    mov sp, bp                                ; 89 ec                       ; 0xc2530 vgabios.c:1592
    pop bp                                    ; 5d                          ; 0xc2532
    pop di                                    ; 5f                          ; 0xc2533
    pop si                                    ; 5e                          ; 0xc2534
    retn 00004h                               ; c2 04 00                    ; 0xc2535
  ; disGetNextSymbol 0xc2538 LB 0x20d5 -> off=0x0 cb=00000000000000a1 uValue=00000000000c2538 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc2538 LB 0xa1
    push si                                   ; 56                          ; 0xc2538 vgabios.c:1595
    push di                                   ; 57                          ; 0xc2539
    push bp                                   ; 55                          ; 0xc253a
    mov bp, sp                                ; 89 e5                       ; 0xc253b
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc253d
    mov bh, al                                ; 88 c7                       ; 0xc2540
    mov ch, dl                                ; 88 d5                       ; 0xc2542
    mov al, cl                                ; 88 c8                       ; 0xc2544
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc2546 vgabios.c:1602
    xor ah, ah                                ; 30 e4                       ; 0xc2549 vgabios.c:1603
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc254b
    xor dh, dh                                ; 30 f6                       ; 0xc254e
    imul dx                                   ; f7 ea                       ; 0xc2550
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2552
    mov dx, ax                                ; 89 c2                       ; 0xc2554
    sal dx, CL                                ; d3 e2                       ; 0xc2556
    mov al, bl                                ; 88 d8                       ; 0xc2558
    xor ah, ah                                ; 30 e4                       ; 0xc255a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc255c
    sal ax, CL                                ; d3 e0                       ; 0xc255e
    add ax, dx                                ; 01 d0                       ; 0xc2560
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc2562
    mov al, bh                                ; 88 f8                       ; 0xc2565 vgabios.c:1604
    xor ah, ah                                ; 30 e4                       ; 0xc2567
    sal ax, CL                                ; d3 e0                       ; 0xc2569
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc256b
    xor bl, bl                                ; 30 db                       ; 0xc256e vgabios.c:1605
    jmp short 025b4h                          ; eb 42                       ; 0xc2570
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc2572 vgabios.c:1609
    jnc short 025adh                          ; 73 37                       ; 0xc2574
    xor bh, bh                                ; 30 ff                       ; 0xc2576 vgabios.c:1611
    mov dl, bl                                ; 88 da                       ; 0xc2578 vgabios.c:1612
    xor dh, dh                                ; 30 f6                       ; 0xc257a
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc257c
    mov si, di                                ; 89 fe                       ; 0xc257f
    add si, dx                                ; 01 d6                       ; 0xc2581
    mov dl, byte [si]                         ; 8a 14                       ; 0xc2583
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc2585
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc2588
    mov dl, ah                                ; 88 e2                       ; 0xc258b
    xor dh, dh                                ; 30 f6                       ; 0xc258d
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc258f
    je short 02596h                           ; 74 02                       ; 0xc2592
    mov bh, ch                                ; 88 ef                       ; 0xc2594 vgabios.c:1614
    mov dl, al                                ; 88 c2                       ; 0xc2596 vgabios.c:1616
    xor dh, dh                                ; 30 f6                       ; 0xc2598
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc259a
    add si, dx                                ; 01 d6                       ; 0xc259d
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc259f vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc25a2
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc25a4
    shr ah, 1                                 ; d0 ec                       ; 0xc25a7 vgabios.c:1617
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc25a9 vgabios.c:1618
    jmp short 02572h                          ; eb c5                       ; 0xc25ab
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc25ad vgabios.c:1619
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc25af
    jnc short 025d1h                          ; 73 1d                       ; 0xc25b2
    mov al, bl                                ; 88 d8                       ; 0xc25b4
    xor ah, ah                                ; 30 e4                       ; 0xc25b6
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc25b8
    xor dh, dh                                ; 30 f6                       ; 0xc25bb
    imul dx                                   ; f7 ea                       ; 0xc25bd
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc25bf
    sal ax, CL                                ; d3 e0                       ; 0xc25c1
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc25c3
    add dx, ax                                ; 01 c2                       ; 0xc25c6
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc25c8
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc25cb
    xor al, al                                ; 30 c0                       ; 0xc25cd
    jmp short 02576h                          ; eb a5                       ; 0xc25cf
    mov sp, bp                                ; 89 ec                       ; 0xc25d1 vgabios.c:1620
    pop bp                                    ; 5d                          ; 0xc25d3
    pop di                                    ; 5f                          ; 0xc25d4
    pop si                                    ; 5e                          ; 0xc25d5
    retn 00002h                               ; c2 02 00                    ; 0xc25d6
  ; disGetNextSymbol 0xc25d9 LB 0x2034 -> off=0x0 cb=0000000000000172 uValue=00000000000c25d9 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc25d9 LB 0x172
    push bp                                   ; 55                          ; 0xc25d9 vgabios.c:1623
    mov bp, sp                                ; 89 e5                       ; 0xc25da
    push si                                   ; 56                          ; 0xc25dc
    push di                                   ; 57                          ; 0xc25dd
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc25de
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc25e1
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc25e4
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc25e7
    mov si, cx                                ; 89 ce                       ; 0xc25ea
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc25ec vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25ef
    mov es, ax                                ; 8e c0                       ; 0xc25f2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc25f4
    xor ah, ah                                ; 30 e4                       ; 0xc25f7 vgabios.c:1631
    call 03940h                               ; e8 44 13                    ; 0xc25f9
    mov cl, al                                ; 88 c1                       ; 0xc25fc
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc25fe
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2601 vgabios.c:1632
    jne short 02608h                          ; 75 03                       ; 0xc2603
    jmp near 02744h                           ; e9 3c 01                    ; 0xc2605
    mov al, dl                                ; 88 d0                       ; 0xc2608 vgabios.c:1635
    xor ah, ah                                ; 30 e4                       ; 0xc260a
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc260c
    lea dx, [bp-01eh]                         ; 8d 56 e2                    ; 0xc260f
    call 00a97h                               ; e8 82 e4                    ; 0xc2612
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2615 vgabios.c:1636
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2618
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc261b
    mov al, ah                                ; 88 e0                       ; 0xc261e
    xor ah, ah                                ; 30 e4                       ; 0xc2620
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2622
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2625
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2628
    mov bx, 00084h                            ; bb 84 00                    ; 0xc262b vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc262e
    mov es, ax                                ; 8e c0                       ; 0xc2631
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2633
    xor ah, ah                                ; 30 e4                       ; 0xc2636 vgabios.c:48
    mov dx, ax                                ; 89 c2                       ; 0xc2638
    inc dx                                    ; 42                          ; 0xc263a
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc263b vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc263e
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2641
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2644 vgabios.c:58
    mov bl, cl                                ; 88 cb                       ; 0xc2647 vgabios.c:1642
    xor bh, bh                                ; 30 ff                       ; 0xc2649
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc264b
    mov di, bx                                ; 89 df                       ; 0xc264d
    sal di, CL                                ; d3 e7                       ; 0xc264f
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc2651
    jne short 02698h                          ; 75 40                       ; 0xc2656
    mul dx                                    ; f7 e2                       ; 0xc2658 vgabios.c:1645
    sal ax, 1                                 ; d1 e0                       ; 0xc265a
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc265c
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc265e
    xor dh, dh                                ; 30 f6                       ; 0xc2661
    inc ax                                    ; 40                          ; 0xc2663
    mul dx                                    ; f7 e2                       ; 0xc2664
    mov bx, ax                                ; 89 c3                       ; 0xc2666
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2668
    xor ah, ah                                ; 30 e4                       ; 0xc266b
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc266d
    mov dx, ax                                ; 89 c2                       ; 0xc2670
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2672
    xor ah, ah                                ; 30 e4                       ; 0xc2675
    add ax, dx                                ; 01 d0                       ; 0xc2677
    sal ax, 1                                 ; d1 e0                       ; 0xc2679
    add bx, ax                                ; 01 c3                       ; 0xc267b
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc267d vgabios.c:1647
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2680
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc2683
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2686 vgabios.c:1648
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2689
    mov cx, si                                ; 89 f1                       ; 0xc268d
    mov di, bx                                ; 89 df                       ; 0xc268f
    jcxz 02695h                               ; e3 02                       ; 0xc2691
    rep stosw                                 ; f3 ab                       ; 0xc2693
    jmp near 02744h                           ; e9 ac 00                    ; 0xc2695 vgabios.c:1650
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc2698 vgabios.c:1653
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc269c
    sal bx, CL                                ; d3 e3                       ; 0xc269e
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc26a0
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc26a4
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc26a7 vgabios.c:1654
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc26ab
    dec si                                    ; 4e                          ; 0xc26ae vgabios.c:1655
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc26af
    je short 02700h                           ; 74 4c                       ; 0xc26b2
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc26b4 vgabios.c:1657
    xor bh, bh                                ; 30 ff                       ; 0xc26b7
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc26b9
    sal bx, CL                                ; d3 e3                       ; 0xc26bb
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc26bd
    cmp al, cl                                ; 38 c8                       ; 0xc26c1
    jc short 026d1h                           ; 72 0c                       ; 0xc26c3
    jbe short 026d7h                          ; 76 10                       ; 0xc26c5
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc26c7
    je short 02723h                           ; 74 58                       ; 0xc26c9
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc26cb
    je short 026dbh                           ; 74 0c                       ; 0xc26cd
    jmp short 0273eh                          ; eb 6d                       ; 0xc26cf
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc26d1
    je short 02702h                           ; 74 2d                       ; 0xc26d3
    jmp short 0273eh                          ; eb 67                       ; 0xc26d5
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc26d7 vgabios.c:1660
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc26db vgabios.c:1662
    xor ah, ah                                ; 30 e4                       ; 0xc26de
    push ax                                   ; 50                          ; 0xc26e0
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc26e1
    push ax                                   ; 50                          ; 0xc26e4
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc26e5
    push ax                                   ; 50                          ; 0xc26e8
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc26e9
    xor ch, ch                                ; 30 ed                       ; 0xc26ec
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc26ee
    xor bh, bh                                ; 30 ff                       ; 0xc26f1
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc26f3
    xor dh, dh                                ; 30 f6                       ; 0xc26f6
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc26f8
    call 02314h                               ; e8 16 fc                    ; 0xc26fb
    jmp short 0273eh                          ; eb 3e                       ; 0xc26fe vgabios.c:1663
    jmp short 02744h                          ; eb 42                       ; 0xc2700
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2702 vgabios.c:1665
    xor ah, ah                                ; 30 e4                       ; 0xc2705
    push ax                                   ; 50                          ; 0xc2707
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2708
    push ax                                   ; 50                          ; 0xc270b
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc270c
    xor ch, ch                                ; 30 ed                       ; 0xc270f
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2711
    xor bh, bh                                ; 30 ff                       ; 0xc2714
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2716
    xor dh, dh                                ; 30 f6                       ; 0xc2719
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc271b
    call 02426h                               ; e8 05 fd                    ; 0xc271e
    jmp short 0273eh                          ; eb 1b                       ; 0xc2721 vgabios.c:1666
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2723 vgabios.c:1668
    xor ah, ah                                ; 30 e4                       ; 0xc2726
    push ax                                   ; 50                          ; 0xc2728
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2729
    xor ch, ch                                ; 30 ed                       ; 0xc272c
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc272e
    xor bh, bh                                ; 30 ff                       ; 0xc2731
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2733
    xor dh, dh                                ; 30 f6                       ; 0xc2736
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2738
    call 02538h                               ; e8 fa fd                    ; 0xc273b
    inc byte [bp-00ah]                        ; fe 46 f6                    ; 0xc273e vgabios.c:1675
    jmp near 026aeh                           ; e9 6a ff                    ; 0xc2741 vgabios.c:1676
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2744 vgabios.c:1678
    pop di                                    ; 5f                          ; 0xc2747
    pop si                                    ; 5e                          ; 0xc2748
    pop bp                                    ; 5d                          ; 0xc2749
    retn                                      ; c3                          ; 0xc274a
  ; disGetNextSymbol 0xc274b LB 0x1ec2 -> off=0x0 cb=0000000000000183 uValue=00000000000c274b 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc274b LB 0x183
    push bp                                   ; 55                          ; 0xc274b vgabios.c:1681
    mov bp, sp                                ; 89 e5                       ; 0xc274c
    push si                                   ; 56                          ; 0xc274e
    push di                                   ; 57                          ; 0xc274f
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2750
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2753
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2756
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc2759
    mov si, cx                                ; 89 ce                       ; 0xc275c
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc275e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2761
    mov es, ax                                ; 8e c0                       ; 0xc2764
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2766
    xor ah, ah                                ; 30 e4                       ; 0xc2769 vgabios.c:1689
    call 03940h                               ; e8 d2 11                    ; 0xc276b
    mov cl, al                                ; 88 c1                       ; 0xc276e
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2770
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2773 vgabios.c:1690
    jne short 0277ah                          ; 75 03                       ; 0xc2775
    jmp near 028c7h                           ; e9 4d 01                    ; 0xc2777
    mov al, dl                                ; 88 d0                       ; 0xc277a vgabios.c:1693
    xor ah, ah                                ; 30 e4                       ; 0xc277c
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc277e
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc2781
    call 00a97h                               ; e8 10 e3                    ; 0xc2784
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc2787 vgabios.c:1694
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc278a
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc278d
    mov al, ah                                ; 88 e0                       ; 0xc2790
    xor ah, ah                                ; 30 e4                       ; 0xc2792
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2794
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2797
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc279a
    mov bx, 00084h                            ; bb 84 00                    ; 0xc279d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc27a0
    mov es, ax                                ; 8e c0                       ; 0xc27a3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc27a5
    xor ah, ah                                ; 30 e4                       ; 0xc27a8 vgabios.c:48
    mov dx, ax                                ; 89 c2                       ; 0xc27aa
    inc dx                                    ; 42                          ; 0xc27ac
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc27ad vgabios.c:57
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc27b0
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc27b3 vgabios.c:58
    mov al, cl                                ; 88 c8                       ; 0xc27b6 vgabios.c:1700
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc27b8
    mov bx, ax                                ; 89 c3                       ; 0xc27ba
    sal bx, CL                                ; d3 e3                       ; 0xc27bc
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc27be
    jne short 0280ah                          ; 75 45                       ; 0xc27c3
    mov ax, di                                ; 89 f8                       ; 0xc27c5 vgabios.c:1703
    mul dx                                    ; f7 e2                       ; 0xc27c7
    sal ax, 1                                 ; d1 e0                       ; 0xc27c9
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc27cb
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc27cd
    xor dh, dh                                ; 30 f6                       ; 0xc27d0
    inc ax                                    ; 40                          ; 0xc27d2
    mul dx                                    ; f7 e2                       ; 0xc27d3
    mov bx, ax                                ; 89 c3                       ; 0xc27d5
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc27d7
    xor ah, ah                                ; 30 e4                       ; 0xc27da
    mul di                                    ; f7 e7                       ; 0xc27dc
    mov dx, ax                                ; 89 c2                       ; 0xc27de
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc27e0
    xor ah, ah                                ; 30 e4                       ; 0xc27e3
    add ax, dx                                ; 01 d0                       ; 0xc27e5
    sal ax, 1                                 ; d1 e0                       ; 0xc27e7
    add bx, ax                                ; 01 c3                       ; 0xc27e9
    dec si                                    ; 4e                          ; 0xc27eb vgabios.c:1705
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc27ec
    je short 02777h                           ; 74 86                       ; 0xc27ef
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc27f1 vgabios.c:1706
    xor ah, ah                                ; 30 e4                       ; 0xc27f4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc27f6
    mov di, ax                                ; 89 c7                       ; 0xc27f8
    sal di, CL                                ; d3 e7                       ; 0xc27fa
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc27fc vgabios.c:50
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2800 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2803
    inc bx                                    ; 43                          ; 0xc2806 vgabios.c:1707
    inc bx                                    ; 43                          ; 0xc2807
    jmp short 027ebh                          ; eb e1                       ; 0xc2808 vgabios.c:1708
    mov di, ax                                ; 89 c7                       ; 0xc280a vgabios.c:1713
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc280c
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2810
    mov di, ax                                ; 89 c7                       ; 0xc2812
    sal di, CL                                ; d3 e7                       ; 0xc2814
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc2816
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc281a
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc281d vgabios.c:1714
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2821
    dec si                                    ; 4e                          ; 0xc2824 vgabios.c:1715
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2825
    je short 0287ah                           ; 74 50                       ; 0xc2828
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc282a vgabios.c:1717
    xor bh, bh                                ; 30 ff                       ; 0xc282d
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc282f
    sal bx, CL                                ; d3 e3                       ; 0xc2831
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2833
    cmp bl, cl                                ; 38 cb                       ; 0xc2837
    jc short 0284ah                           ; 72 0f                       ; 0xc2839
    jbe short 02851h                          ; 76 14                       ; 0xc283b
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc283d
    je short 028a6h                           ; 74 64                       ; 0xc2840
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2842
    je short 02855h                           ; 74 0e                       ; 0xc2845
    jmp near 028c1h                           ; e9 77 00                    ; 0xc2847
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc284a
    je short 0287ch                           ; 74 2d                       ; 0xc284d
    jmp short 028c1h                          ; eb 70                       ; 0xc284f
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2851 vgabios.c:1720
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2855 vgabios.c:1722
    xor ah, ah                                ; 30 e4                       ; 0xc2858
    push ax                                   ; 50                          ; 0xc285a
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc285b
    push ax                                   ; 50                          ; 0xc285e
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc285f
    push ax                                   ; 50                          ; 0xc2862
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2863
    xor ch, ch                                ; 30 ed                       ; 0xc2866
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2868
    xor bh, bh                                ; 30 ff                       ; 0xc286b
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc286d
    xor dh, dh                                ; 30 f6                       ; 0xc2870
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2872
    call 02314h                               ; e8 9c fa                    ; 0xc2875
    jmp short 028c1h                          ; eb 47                       ; 0xc2878 vgabios.c:1723
    jmp short 028c7h                          ; eb 4b                       ; 0xc287a
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc287c vgabios.c:1725
    xor ah, ah                                ; 30 e4                       ; 0xc287f
    push ax                                   ; 50                          ; 0xc2881
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2882
    push ax                                   ; 50                          ; 0xc2885
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2886
    xor ch, ch                                ; 30 ed                       ; 0xc2889
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc288b
    xor bh, bh                                ; 30 ff                       ; 0xc288e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2890
    xor dh, dh                                ; 30 f6                       ; 0xc2893
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2895
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc2898
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc289b
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc289e
    call 02426h                               ; e8 82 fb                    ; 0xc28a1
    jmp short 028c1h                          ; eb 1b                       ; 0xc28a4 vgabios.c:1726
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc28a6 vgabios.c:1728
    xor ah, ah                                ; 30 e4                       ; 0xc28a9
    push ax                                   ; 50                          ; 0xc28ab
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc28ac
    xor ch, ch                                ; 30 ed                       ; 0xc28af
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc28b1
    xor bh, bh                                ; 30 ff                       ; 0xc28b4
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc28b6
    xor dh, dh                                ; 30 f6                       ; 0xc28b9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc28bb
    call 02538h                               ; e8 77 fc                    ; 0xc28be
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc28c1 vgabios.c:1735
    jmp near 02824h                           ; e9 5d ff                    ; 0xc28c4 vgabios.c:1736
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc28c7 vgabios.c:1738
    pop di                                    ; 5f                          ; 0xc28ca
    pop si                                    ; 5e                          ; 0xc28cb
    pop bp                                    ; 5d                          ; 0xc28cc
    retn                                      ; c3                          ; 0xc28cd
  ; disGetNextSymbol 0xc28ce LB 0x1d3f -> off=0x0 cb=000000000000017a uValue=00000000000c28ce 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc28ce LB 0x17a
    push bp                                   ; 55                          ; 0xc28ce vgabios.c:1741
    mov bp, sp                                ; 89 e5                       ; 0xc28cf
    push si                                   ; 56                          ; 0xc28d1
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc28d2
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc28d5
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc28d8
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc28db
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc28de
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc28e1 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28e4
    mov es, ax                                ; 8e c0                       ; 0xc28e7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc28e9
    xor ah, ah                                ; 30 e4                       ; 0xc28ec vgabios.c:1748
    call 03940h                               ; e8 4f 10                    ; 0xc28ee
    mov ch, al                                ; 88 c5                       ; 0xc28f1
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc28f3 vgabios.c:1749
    je short 0291eh                           ; 74 27                       ; 0xc28f5
    mov bl, al                                ; 88 c3                       ; 0xc28f7 vgabios.c:1750
    xor bh, bh                                ; 30 ff                       ; 0xc28f9
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc28fb
    sal bx, CL                                ; d3 e3                       ; 0xc28fd
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc28ff
    je short 0291eh                           ; 74 18                       ; 0xc2904
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc2906 vgabios.c:1752
    cmp al, cl                                ; 38 c8                       ; 0xc290a
    jc short 0291ah                           ; 72 0c                       ; 0xc290c
    jbe short 02924h                          ; 76 14                       ; 0xc290e
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2910
    je short 02921h                           ; 74 0d                       ; 0xc2912
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2914
    je short 02924h                           ; 74 0c                       ; 0xc2916
    jmp short 0291eh                          ; eb 04                       ; 0xc2918
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc291a
    je short 02996h                           ; 74 78                       ; 0xc291c
    jmp near 02a21h                           ; e9 00 01                    ; 0xc291e
    jmp near 02a27h                           ; e9 03 01                    ; 0xc2921
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2924 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2927
    mov es, ax                                ; 8e c0                       ; 0xc292a
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc292c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc292f vgabios.c:58
    mul dx                                    ; f7 e2                       ; 0xc2932
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2934
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2936
    shr bx, CL                                ; d3 eb                       ; 0xc2939
    add bx, ax                                ; 01 c3                       ; 0xc293b
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc293d vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2940
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2943 vgabios.c:58
    xor dh, dh                                ; 30 f6                       ; 0xc2946
    mul dx                                    ; f7 e2                       ; 0xc2948
    add bx, ax                                ; 01 c3                       ; 0xc294a
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc294c vgabios.c:1758
    and cl, 007h                              ; 80 e1 07                    ; 0xc294f
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2952
    sar ax, CL                                ; d3 f8                       ; 0xc2955
    mov ah, al                                ; 88 c4                       ; 0xc2957 vgabios.c:1759
    xor al, al                                ; 30 c0                       ; 0xc2959
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc295b
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc295d
    out DX, ax                                ; ef                          ; 0xc2960
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2961 vgabios.c:1760
    out DX, ax                                ; ef                          ; 0xc2964
    mov dx, bx                                ; 89 da                       ; 0xc2965 vgabios.c:1761
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2967
    call 0396bh                               ; e8 fe 0f                    ; 0xc296a
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc296d vgabios.c:1762
    je short 0297ah                           ; 74 07                       ; 0xc2971
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2973 vgabios.c:1764
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2976
    out DX, ax                                ; ef                          ; 0xc2979
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc297a vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc297d
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc297f
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2982
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2985 vgabios.c:1767
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2988
    out DX, ax                                ; ef                          ; 0xc298b
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc298c vgabios.c:1768
    out DX, ax                                ; ef                          ; 0xc298f
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2990 vgabios.c:1769
    out DX, ax                                ; ef                          ; 0xc2993
    jmp short 0291eh                          ; eb 88                       ; 0xc2994 vgabios.c:1770
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2996 vgabios.c:1772
    shr ax, 1                                 ; d1 e8                       ; 0xc2999
    mov dx, strict word 00050h                ; ba 50 00                    ; 0xc299b
    mul dx                                    ; f7 e2                       ; 0xc299e
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc29a0
    jne short 029b0h                          ; 75 09                       ; 0xc29a5
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc29a7 vgabios.c:1774
    shr bx, 1                                 ; d1 eb                       ; 0xc29aa
    shr bx, 1                                 ; d1 eb                       ; 0xc29ac
    jmp short 029b5h                          ; eb 05                       ; 0xc29ae vgabios.c:1776
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc29b0 vgabios.c:1778
    shr bx, CL                                ; d3 eb                       ; 0xc29b3
    add bx, ax                                ; 01 c3                       ; 0xc29b5
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc29b7 vgabios.c:1780
    je short 029c0h                           ; 74 03                       ; 0xc29bb
    add bh, 020h                              ; 80 c7 20                    ; 0xc29bd
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc29c0 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc29c3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc29c5
    mov dl, ch                                ; 88 ea                       ; 0xc29c8 vgabios.c:1782
    xor dh, dh                                ; 30 f6                       ; 0xc29ca
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc29cc
    mov si, dx                                ; 89 d6                       ; 0xc29ce
    sal si, CL                                ; d3 e6                       ; 0xc29d0
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc29d2
    jne short 029f3h                          ; 75 1a                       ; 0xc29d7
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc29d9 vgabios.c:1784
    and ah, cl                                ; 20 cc                       ; 0xc29dc
    mov dl, cl                                ; 88 ca                       ; 0xc29de
    sub dl, ah                                ; 28 e2                       ; 0xc29e0
    mov ah, dl                                ; 88 d4                       ; 0xc29e2
    sal ah, 1                                 ; d0 e4                       ; 0xc29e4
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc29e6
    and dl, cl                                ; 20 ca                       ; 0xc29e9
    mov cl, ah                                ; 88 e1                       ; 0xc29eb
    sal dl, CL                                ; d2 e2                       ; 0xc29ed
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc29ef vgabios.c:1785
    jmp short 02a07h                          ; eb 14                       ; 0xc29f1 vgabios.c:1787
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc29f3 vgabios.c:1789
    and ah, 007h                              ; 80 e4 07                    ; 0xc29f6
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc29f9
    sub cl, ah                                ; 28 e1                       ; 0xc29fb
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc29fd
    and dl, 001h                              ; 80 e2 01                    ; 0xc2a00
    sal dl, CL                                ; d2 e2                       ; 0xc2a03
    mov AH, strict byte 001h                  ; b4 01                       ; 0xc2a05 vgabios.c:1790
    sal ah, CL                                ; d2 e4                       ; 0xc2a07
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2a09 vgabios.c:1792
    je short 02a13h                           ; 74 04                       ; 0xc2a0d
    xor al, dl                                ; 30 d0                       ; 0xc2a0f vgabios.c:1794
    jmp short 02a19h                          ; eb 06                       ; 0xc2a11 vgabios.c:1796
    not ah                                    ; f6 d4                       ; 0xc2a13 vgabios.c:1798
    and al, ah                                ; 20 e0                       ; 0xc2a15
    or al, dl                                 ; 08 d0                       ; 0xc2a17 vgabios.c:1799
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2a19 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc2a1c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2a1e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a21 vgabios.c:1802
    pop si                                    ; 5e                          ; 0xc2a24
    pop bp                                    ; 5d                          ; 0xc2a25
    retn                                      ; c3                          ; 0xc2a26
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2a27 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a2a
    mov es, ax                                ; 8e c0                       ; 0xc2a2d
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2a2f
    sal dx, CL                                ; d3 e2                       ; 0xc2a32 vgabios.c:58
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2a34
    mul dx                                    ; f7 e2                       ; 0xc2a37
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2a39
    add bx, ax                                ; 01 c3                       ; 0xc2a3c
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2a3e vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc2a41
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2a43
    jmp short 02a1eh                          ; eb d6                       ; 0xc2a46
  ; disGetNextSymbol 0xc2a48 LB 0x1bc5 -> off=0x0 cb=0000000000000263 uValue=00000000000c2a48 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc2a48 LB 0x263
    push bp                                   ; 55                          ; 0xc2a48 vgabios.c:1815
    mov bp, sp                                ; 89 e5                       ; 0xc2a49
    push si                                   ; 56                          ; 0xc2a4b
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2a4c
    mov ch, al                                ; 88 c5                       ; 0xc2a4f
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2a51
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc2a54
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc2a57
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2a5a vgabios.c:1823
    jne short 02a6dh                          ; 75 0e                       ; 0xc2a5d
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc2a5f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a62
    mov es, ax                                ; 8e c0                       ; 0xc2a65
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2a67
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2a6a vgabios.c:48
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2a6d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a70
    mov es, ax                                ; 8e c0                       ; 0xc2a73
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2a75
    xor ah, ah                                ; 30 e4                       ; 0xc2a78 vgabios.c:1828
    call 03940h                               ; e8 c3 0e                    ; 0xc2a7a
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2a7d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2a80 vgabios.c:1829
    je short 02ae9h                           ; 74 65                       ; 0xc2a82
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2a84 vgabios.c:1832
    xor ah, ah                                ; 30 e4                       ; 0xc2a87
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc2a89
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc2a8c
    call 00a97h                               ; e8 05 e0                    ; 0xc2a8f
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2a92 vgabios.c:1833
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2a95
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc2a98
    mov al, ah                                ; 88 e0                       ; 0xc2a9b
    xor ah, ah                                ; 30 e4                       ; 0xc2a9d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2a9f
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2aa2 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2aa5
    mov es, dx                                ; 8e c2                       ; 0xc2aa8
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2aaa
    xor dh, dh                                ; 30 f6                       ; 0xc2aad vgabios.c:48
    inc dx                                    ; 42                          ; 0xc2aaf
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc2ab0
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2ab3 vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2ab6
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc2ab9 vgabios.c:58
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2abc vgabios.c:1839
    jc short 02acfh                           ; 72 0e                       ; 0xc2abf
    jbe short 02ad7h                          ; 76 14                       ; 0xc2ac1
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc2ac3
    je short 02aech                           ; 74 24                       ; 0xc2ac6
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc2ac8
    je short 02ae2h                           ; 74 15                       ; 0xc2acb
    jmp short 02af2h                          ; eb 23                       ; 0xc2acd
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2acf
    jne short 02af2h                          ; 75 1e                       ; 0xc2ad2
    jmp near 02bfah                           ; e9 23 01                    ; 0xc2ad4
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2ad7 vgabios.c:1846
    jbe short 02aefh                          ; 76 12                       ; 0xc2adb
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2add
    jmp short 02aefh                          ; eb 0d                       ; 0xc2ae0 vgabios.c:1847
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2ae2 vgabios.c:1850
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2ae4
    jmp short 02aefh                          ; eb 06                       ; 0xc2ae7 vgabios.c:1851
    jmp near 02ca5h                           ; e9 b9 01                    ; 0xc2ae9
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2aec vgabios.c:1854
    jmp near 02bfah                           ; e9 08 01                    ; 0xc2aef vgabios.c:1855
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2af2 vgabios.c:1859
    xor ah, ah                                ; 30 e4                       ; 0xc2af5
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2af7
    mov bx, ax                                ; 89 c3                       ; 0xc2af9
    sal bx, CL                                ; d3 e3                       ; 0xc2afb
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2afd
    jne short 02b46h                          ; 75 42                       ; 0xc2b02
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2b04 vgabios.c:1862
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc2b07
    sal ax, 1                                 ; d1 e0                       ; 0xc2b0a
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2b0c
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2b0e
    xor dh, dh                                ; 30 f6                       ; 0xc2b11
    inc ax                                    ; 40                          ; 0xc2b13
    mul dx                                    ; f7 e2                       ; 0xc2b14
    mov si, ax                                ; 89 c6                       ; 0xc2b16
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2b18
    xor ah, ah                                ; 30 e4                       ; 0xc2b1b
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2b1d
    mov dx, ax                                ; 89 c2                       ; 0xc2b20
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2b22
    xor ah, ah                                ; 30 e4                       ; 0xc2b25
    add ax, dx                                ; 01 d0                       ; 0xc2b27
    sal ax, 1                                 ; d1 e0                       ; 0xc2b29
    add si, ax                                ; 01 c6                       ; 0xc2b2b
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2b2d vgabios.c:50
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc2b31 vgabios.c:52
    cmp cl, byte [bp-004h]                    ; 3a 4e fc                    ; 0xc2b34 vgabios.c:1867
    jne short 02b76h                          ; 75 3d                       ; 0xc2b37
    inc si                                    ; 46                          ; 0xc2b39 vgabios.c:1868
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2b3a vgabios.c:50
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2b3e
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2b41
    jmp short 02b76h                          ; eb 30                       ; 0xc2b44 vgabios.c:1870
    mov si, ax                                ; 89 c6                       ; 0xc2b46 vgabios.c:1873
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc2b48
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2b4c
    mov si, ax                                ; 89 c6                       ; 0xc2b4e
    sal si, CL                                ; d3 e6                       ; 0xc2b50
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc2b52
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2b56 vgabios.c:1874
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2b5a vgabios.c:1875
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2b5e
    jc short 02b71h                           ; 72 0e                       ; 0xc2b61
    jbe short 02b78h                          ; 76 13                       ; 0xc2b63
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2b65
    je short 02bc8h                           ; 74 5e                       ; 0xc2b68
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2b6a
    je short 02b7ch                           ; 74 0d                       ; 0xc2b6d
    jmp short 02be7h                          ; eb 76                       ; 0xc2b6f
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2b71
    je short 02ba6h                           ; 74 30                       ; 0xc2b74
    jmp short 02be7h                          ; eb 6f                       ; 0xc2b76
    or byte [bp-00ch], 001h                   ; 80 4e f4 01                 ; 0xc2b78 vgabios.c:1878
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2b7c vgabios.c:1880
    xor ah, ah                                ; 30 e4                       ; 0xc2b7f
    push ax                                   ; 50                          ; 0xc2b81
    mov al, dl                                ; 88 d0                       ; 0xc2b82
    push ax                                   ; 50                          ; 0xc2b84
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2b85
    push ax                                   ; 50                          ; 0xc2b88
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2b89
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2b8c
    xor bh, bh                                ; 30 ff                       ; 0xc2b8f
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc2b91
    xor dh, dh                                ; 30 f6                       ; 0xc2b94
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc2b96
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc2b99
    mov cx, ax                                ; 89 c1                       ; 0xc2b9c
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc2b9e
    call 02314h                               ; e8 70 f7                    ; 0xc2ba1
    jmp short 02be7h                          ; eb 41                       ; 0xc2ba4 vgabios.c:1881
    push ax                                   ; 50                          ; 0xc2ba6 vgabios.c:1883
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2ba7
    push ax                                   ; 50                          ; 0xc2baa
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2bab
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2bae
    xor bh, bh                                ; 30 ff                       ; 0xc2bb1
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc2bb3
    xor dh, dh                                ; 30 f6                       ; 0xc2bb6
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc2bb8
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc2bbb
    mov cx, ax                                ; 89 c1                       ; 0xc2bbe
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc2bc0
    call 02426h                               ; e8 60 f8                    ; 0xc2bc3
    jmp short 02be7h                          ; eb 1f                       ; 0xc2bc6 vgabios.c:1884
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2bc8 vgabios.c:1886
    push ax                                   ; 50                          ; 0xc2bcb
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2bcc
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2bcf
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc2bd2
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2bd5
    xor bh, bh                                ; 30 ff                       ; 0xc2bd8
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc2bda
    xor dh, dh                                ; 30 f6                       ; 0xc2bdd
    mov al, ch                                ; 88 e8                       ; 0xc2bdf
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc2be1
    call 02538h                               ; e8 51 f9                    ; 0xc2be4
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2be7 vgabios.c:1894
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2bea vgabios.c:1896
    xor ah, ah                                ; 30 e4                       ; 0xc2bed
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc2bef
    jne short 02bfah                          ; 75 06                       ; 0xc2bf2
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2bf4 vgabios.c:1897
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc2bf7 vgabios.c:1898
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2bfa vgabios.c:1903
    xor ah, ah                                ; 30 e4                       ; 0xc2bfd
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc2bff
    jne short 02c68h                          ; 75 64                       ; 0xc2c02
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2c04 vgabios.c:1905
    xor bh, bh                                ; 30 ff                       ; 0xc2c07
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2c09
    sal bx, CL                                ; d3 e3                       ; 0xc2c0b
    mov cl, byte [bp-014h]                    ; 8a 4e ec                    ; 0xc2c0d
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc2c10
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc2c12
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc2c15
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2c17
    jne short 02c6ah                          ; 75 4c                       ; 0xc2c1c
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2c1e vgabios.c:1907
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc2c21
    sal ax, 1                                 ; d1 e0                       ; 0xc2c24
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2c26
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2c28
    xor dh, dh                                ; 30 f6                       ; 0xc2c2b
    inc ax                                    ; 40                          ; 0xc2c2d
    mul dx                                    ; f7 e2                       ; 0xc2c2e
    mov si, ax                                ; 89 c6                       ; 0xc2c30
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c32
    xor ah, ah                                ; 30 e4                       ; 0xc2c35
    dec ax                                    ; 48                          ; 0xc2c37
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2c38
    mov dx, ax                                ; 89 c2                       ; 0xc2c3b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2c3d
    xor ah, ah                                ; 30 e4                       ; 0xc2c40
    add ax, dx                                ; 01 d0                       ; 0xc2c42
    sal ax, 1                                 ; d1 e0                       ; 0xc2c44
    add si, ax                                ; 01 c6                       ; 0xc2c46
    inc si                                    ; 46                          ; 0xc2c48 vgabios.c:1908
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2c49 vgabios.c:45
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2c4d vgabios.c:47
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2c50 vgabios.c:1909
    push ax                                   ; 50                          ; 0xc2c53
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2c54
    xor ah, ah                                ; 30 e4                       ; 0xc2c57
    push ax                                   ; 50                          ; 0xc2c59
    mov al, ch                                ; 88 e8                       ; 0xc2c5a
    push ax                                   ; 50                          ; 0xc2c5c
    mov al, cl                                ; 88 c8                       ; 0xc2c5d
    push ax                                   ; 50                          ; 0xc2c5f
    xor dh, dh                                ; 30 f6                       ; 0xc2c60
    xor cx, cx                                ; 31 c9                       ; 0xc2c62
    xor bx, bx                                ; 31 db                       ; 0xc2c64
    jmp short 02c80h                          ; eb 18                       ; 0xc2c66 vgabios.c:1911
    jmp short 02c89h                          ; eb 1f                       ; 0xc2c68
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2c6a vgabios.c:1913
    push ax                                   ; 50                          ; 0xc2c6d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2c6e
    xor ah, ah                                ; 30 e4                       ; 0xc2c71
    push ax                                   ; 50                          ; 0xc2c73
    mov al, ch                                ; 88 e8                       ; 0xc2c74
    push ax                                   ; 50                          ; 0xc2c76
    mov al, cl                                ; 88 c8                       ; 0xc2c77
    push ax                                   ; 50                          ; 0xc2c79
    xor cx, cx                                ; 31 c9                       ; 0xc2c7a
    xor bx, bx                                ; 31 db                       ; 0xc2c7c
    xor dx, dx                                ; 31 d2                       ; 0xc2c7e
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2c80
    call 01c71h                               ; e8 eb ef                    ; 0xc2c83
    dec byte [bp-008h]                        ; fe 4e f8                    ; 0xc2c86 vgabios.c:1915
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c89 vgabios.c:1919
    xor ah, ah                                ; 30 e4                       ; 0xc2c8c
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2c8e
    mov CL, strict byte 008h                  ; b1 08                       ; 0xc2c91
    sal word [bp-016h], CL                    ; d3 66 ea                    ; 0xc2c93
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2c96
    add word [bp-016h], ax                    ; 01 46 ea                    ; 0xc2c99
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc2c9c vgabios.c:1920
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2c9f
    call 012cfh                               ; e8 2a e6                    ; 0xc2ca2
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2ca5 vgabios.c:1921
    pop si                                    ; 5e                          ; 0xc2ca8
    pop bp                                    ; 5d                          ; 0xc2ca9
    retn                                      ; c3                          ; 0xc2caa
  ; disGetNextSymbol 0xc2cab LB 0x1962 -> off=0x0 cb=000000000000002c uValue=00000000000c2cab 'get_font_access'
get_font_access:                             ; 0xc2cab LB 0x2c
    push bp                                   ; 55                          ; 0xc2cab vgabios.c:1924
    mov bp, sp                                ; 89 e5                       ; 0xc2cac
    push dx                                   ; 52                          ; 0xc2cae
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2caf vgabios.c:1926
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2cb2
    out DX, ax                                ; ef                          ; 0xc2cb5
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2cb6 vgabios.c:1927
    out DX, ax                                ; ef                          ; 0xc2cb9
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2cba vgabios.c:1928
    out DX, ax                                ; ef                          ; 0xc2cbd
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2cbe vgabios.c:1929
    out DX, ax                                ; ef                          ; 0xc2cc1
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2cc2 vgabios.c:1930
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2cc5
    out DX, ax                                ; ef                          ; 0xc2cc8
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2cc9 vgabios.c:1931
    out DX, ax                                ; ef                          ; 0xc2ccc
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2ccd vgabios.c:1932
    out DX, ax                                ; ef                          ; 0xc2cd0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2cd1 vgabios.c:1933
    pop dx                                    ; 5a                          ; 0xc2cd4
    pop bp                                    ; 5d                          ; 0xc2cd5
    retn                                      ; c3                          ; 0xc2cd6
  ; disGetNextSymbol 0xc2cd7 LB 0x1936 -> off=0x0 cb=000000000000003f uValue=00000000000c2cd7 'release_font_access'
release_font_access:                         ; 0xc2cd7 LB 0x3f
    push bp                                   ; 55                          ; 0xc2cd7 vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2cd8
    push dx                                   ; 52                          ; 0xc2cda
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2cdb vgabios.c:1937
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2cde
    out DX, ax                                ; ef                          ; 0xc2ce1
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2ce2 vgabios.c:1938
    out DX, ax                                ; ef                          ; 0xc2ce5
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2ce6 vgabios.c:1939
    out DX, ax                                ; ef                          ; 0xc2ce9
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2cea vgabios.c:1940
    out DX, ax                                ; ef                          ; 0xc2ced
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2cee vgabios.c:1941
    in AL, DX                                 ; ec                          ; 0xc2cf1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2cf2
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2cf4
    sal ax, 1                                 ; d1 e0                       ; 0xc2cf7
    sal ax, 1                                 ; d1 e0                       ; 0xc2cf9
    mov ah, al                                ; 88 c4                       ; 0xc2cfb
    or ah, 00ah                               ; 80 cc 0a                    ; 0xc2cfd
    xor al, al                                ; 30 c0                       ; 0xc2d00
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2d02
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2d04
    out DX, ax                                ; ef                          ; 0xc2d07
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2d08 vgabios.c:1942
    out DX, ax                                ; ef                          ; 0xc2d0b
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2d0c vgabios.c:1943
    out DX, ax                                ; ef                          ; 0xc2d0f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2d10 vgabios.c:1944
    pop dx                                    ; 5a                          ; 0xc2d13
    pop bp                                    ; 5d                          ; 0xc2d14
    retn                                      ; c3                          ; 0xc2d15
  ; disGetNextSymbol 0xc2d16 LB 0x18f7 -> off=0x0 cb=00000000000000b3 uValue=00000000000c2d16 'set_scan_lines'
set_scan_lines:                              ; 0xc2d16 LB 0xb3
    push bp                                   ; 55                          ; 0xc2d16 vgabios.c:1946
    mov bp, sp                                ; 89 e5                       ; 0xc2d17
    push bx                                   ; 53                          ; 0xc2d19
    push cx                                   ; 51                          ; 0xc2d1a
    push dx                                   ; 52                          ; 0xc2d1b
    push si                                   ; 56                          ; 0xc2d1c
    push di                                   ; 57                          ; 0xc2d1d
    mov bl, al                                ; 88 c3                       ; 0xc2d1e
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2d20 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d23
    mov es, ax                                ; 8e c0                       ; 0xc2d26
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2d28
    mov cx, si                                ; 89 f1                       ; 0xc2d2b vgabios.c:58
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2d2d vgabios.c:1952
    mov dx, si                                ; 89 f2                       ; 0xc2d2f
    out DX, AL                                ; ee                          ; 0xc2d31
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2d32 vgabios.c:1953
    in AL, DX                                 ; ec                          ; 0xc2d35
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d36
    mov ah, al                                ; 88 c4                       ; 0xc2d38 vgabios.c:1954
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2d3a
    mov al, bl                                ; 88 d8                       ; 0xc2d3d
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2d3f
    or al, ah                                 ; 08 e0                       ; 0xc2d41
    out DX, AL                                ; ee                          ; 0xc2d43 vgabios.c:1955
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2d44 vgabios.c:1956
    jne short 02d51h                          ; 75 08                       ; 0xc2d47
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2d49 vgabios.c:1958
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2d4c
    jmp short 02d5eh                          ; eb 0d                       ; 0xc2d4f vgabios.c:1960
    mov dl, bl                                ; 88 da                       ; 0xc2d51 vgabios.c:1962
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2d53
    xor dh, dh                                ; 30 f6                       ; 0xc2d56
    mov al, bl                                ; 88 d8                       ; 0xc2d58
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2d5a
    xor ah, ah                                ; 30 e4                       ; 0xc2d5c
    call 011d3h                               ; e8 72 e4                    ; 0xc2d5e
    xor bh, bh                                ; 30 ff                       ; 0xc2d61 vgabios.c:1964
    mov si, 00085h                            ; be 85 00                    ; 0xc2d63 vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d66
    mov es, ax                                ; 8e c0                       ; 0xc2d69
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2d6b
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2d6e vgabios.c:1965
    mov dx, cx                                ; 89 ca                       ; 0xc2d70
    out DX, AL                                ; ee                          ; 0xc2d72
    mov si, cx                                ; 89 ce                       ; 0xc2d73 vgabios.c:1966
    inc si                                    ; 46                          ; 0xc2d75
    mov dx, si                                ; 89 f2                       ; 0xc2d76
    in AL, DX                                 ; ec                          ; 0xc2d78
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d79
    mov di, ax                                ; 89 c7                       ; 0xc2d7b
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2d7d vgabios.c:1967
    mov dx, cx                                ; 89 ca                       ; 0xc2d7f
    out DX, AL                                ; ee                          ; 0xc2d81
    mov dx, si                                ; 89 f2                       ; 0xc2d82 vgabios.c:1968
    in AL, DX                                 ; ec                          ; 0xc2d84
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d85
    mov dl, al                                ; 88 c2                       ; 0xc2d87 vgabios.c:1969
    and dl, 002h                              ; 80 e2 02                    ; 0xc2d89
    xor dh, dh                                ; 30 f6                       ; 0xc2d8c
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2d8e
    sal dx, CL                                ; d3 e2                       ; 0xc2d90
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2d92
    xor ah, ah                                ; 30 e4                       ; 0xc2d94
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2d96
    sal ax, CL                                ; d3 e0                       ; 0xc2d98
    add ax, dx                                ; 01 d0                       ; 0xc2d9a
    inc ax                                    ; 40                          ; 0xc2d9c
    add ax, di                                ; 01 f8                       ; 0xc2d9d
    xor dx, dx                                ; 31 d2                       ; 0xc2d9f vgabios.c:1970
    div bx                                    ; f7 f3                       ; 0xc2da1
    mov dl, al                                ; 88 c2                       ; 0xc2da3 vgabios.c:1971
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2da5
    mov si, 00084h                            ; be 84 00                    ; 0xc2da7 vgabios.c:52
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2daa
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2dad vgabios.c:57
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2db0
    xor ah, ah                                ; 30 e4                       ; 0xc2db3 vgabios.c:1973
    mul dx                                    ; f7 e2                       ; 0xc2db5
    sal ax, 1                                 ; d1 e0                       ; 0xc2db7
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2db9 vgabios.c:62
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2dbc
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2dbf vgabios.c:1974
    pop di                                    ; 5f                          ; 0xc2dc2
    pop si                                    ; 5e                          ; 0xc2dc3
    pop dx                                    ; 5a                          ; 0xc2dc4
    pop cx                                    ; 59                          ; 0xc2dc5
    pop bx                                    ; 5b                          ; 0xc2dc6
    pop bp                                    ; 5d                          ; 0xc2dc7
    retn                                      ; c3                          ; 0xc2dc8
  ; disGetNextSymbol 0xc2dc9 LB 0x1844 -> off=0x0 cb=0000000000000020 uValue=00000000000c2dc9 'biosfn_set_font_block'
biosfn_set_font_block:                       ; 0xc2dc9 LB 0x20
    push bp                                   ; 55                          ; 0xc2dc9 vgabios.c:1976
    mov bp, sp                                ; 89 e5                       ; 0xc2dca
    push bx                                   ; 53                          ; 0xc2dcc
    push dx                                   ; 52                          ; 0xc2dcd
    mov bl, al                                ; 88 c3                       ; 0xc2dce
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2dd0 vgabios.c:1978
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2dd3
    out DX, ax                                ; ef                          ; 0xc2dd6
    mov ah, bl                                ; 88 dc                       ; 0xc2dd7 vgabios.c:1979
    xor al, al                                ; 30 c0                       ; 0xc2dd9
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc2ddb
    out DX, ax                                ; ef                          ; 0xc2ddd
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2dde vgabios.c:1980
    out DX, ax                                ; ef                          ; 0xc2de1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2de2 vgabios.c:1981
    pop dx                                    ; 5a                          ; 0xc2de5
    pop bx                                    ; 5b                          ; 0xc2de6
    pop bp                                    ; 5d                          ; 0xc2de7
    retn                                      ; c3                          ; 0xc2de8
  ; disGetNextSymbol 0xc2de9 LB 0x1824 -> off=0x0 cb=0000000000000084 uValue=00000000000c2de9 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2de9 LB 0x84
    push bp                                   ; 55                          ; 0xc2de9 vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2dea
    push si                                   ; 56                          ; 0xc2dec
    push di                                   ; 57                          ; 0xc2ded
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2dee
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2df1
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc2df4
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2df7
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc2dfa
    call 02cabh                               ; e8 ab fe                    ; 0xc2dfd vgabios.c:1988
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2e00 vgabios.c:1989
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2e03
    xor ah, ah                                ; 30 e4                       ; 0xc2e05
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2e07
    mov bx, ax                                ; 89 c3                       ; 0xc2e09
    sal bx, CL                                ; d3 e3                       ; 0xc2e0b
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2e0d
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2e10
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2e12
    sal ax, CL                                ; d3 e0                       ; 0xc2e14
    add bx, ax                                ; 01 c3                       ; 0xc2e16
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2e18
    xor bx, bx                                ; 31 db                       ; 0xc2e1b vgabios.c:1990
    cmp bx, word [bp-00ch]                    ; 3b 5e f4                    ; 0xc2e1d
    jnc short 02e53h                          ; 73 31                       ; 0xc2e20
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2e22 vgabios.c:1992
    xor ah, ah                                ; 30 e4                       ; 0xc2e25
    mov si, ax                                ; 89 c6                       ; 0xc2e27
    mov ax, bx                                ; 89 d8                       ; 0xc2e29
    mul si                                    ; f7 e6                       ; 0xc2e2b
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc2e2d
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2e30 vgabios.c:1993
    add di, bx                                ; 01 df                       ; 0xc2e33
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2e35
    sal di, CL                                ; d3 e7                       ; 0xc2e37
    add di, word [bp-008h]                    ; 03 7e f8                    ; 0xc2e39
    mov cx, si                                ; 89 f1                       ; 0xc2e3c vgabios.c:1994
    mov si, ax                                ; 89 c6                       ; 0xc2e3e
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2e40
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2e43
    mov es, ax                                ; 8e c0                       ; 0xc2e46
    jcxz 02e50h                               ; e3 06                       ; 0xc2e48
    push DS                                   ; 1e                          ; 0xc2e4a
    mov ds, dx                                ; 8e da                       ; 0xc2e4b
    rep movsb                                 ; f3 a4                       ; 0xc2e4d
    pop DS                                    ; 1f                          ; 0xc2e4f
    inc bx                                    ; 43                          ; 0xc2e50 vgabios.c:1995
    jmp short 02e1dh                          ; eb ca                       ; 0xc2e51
    call 02cd7h                               ; e8 81 fe                    ; 0xc2e53 vgabios.c:1996
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2e56 vgabios.c:1997
    jc short 02e64h                           ; 72 08                       ; 0xc2e5a
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2e5c vgabios.c:1999
    xor ah, ah                                ; 30 e4                       ; 0xc2e5f
    call 02d16h                               ; e8 b2 fe                    ; 0xc2e61
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2e64 vgabios.c:2001
    pop di                                    ; 5f                          ; 0xc2e67
    pop si                                    ; 5e                          ; 0xc2e68
    pop bp                                    ; 5d                          ; 0xc2e69
    retn 00006h                               ; c2 06 00                    ; 0xc2e6a
  ; disGetNextSymbol 0xc2e6d LB 0x17a0 -> off=0x0 cb=0000000000000075 uValue=00000000000c2e6d 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2e6d LB 0x75
    push bp                                   ; 55                          ; 0xc2e6d vgabios.c:2003
    mov bp, sp                                ; 89 e5                       ; 0xc2e6e
    push bx                                   ; 53                          ; 0xc2e70
    push cx                                   ; 51                          ; 0xc2e71
    push si                                   ; 56                          ; 0xc2e72
    push di                                   ; 57                          ; 0xc2e73
    push ax                                   ; 50                          ; 0xc2e74
    push ax                                   ; 50                          ; 0xc2e75
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2e76
    call 02cabh                               ; e8 2f fe                    ; 0xc2e79 vgabios.c:2007
    mov al, dl                                ; 88 d0                       ; 0xc2e7c vgabios.c:2008
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2e7e
    xor ah, ah                                ; 30 e4                       ; 0xc2e80
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2e82
    mov bx, ax                                ; 89 c3                       ; 0xc2e84
    sal bx, CL                                ; d3 e3                       ; 0xc2e86
    mov al, dl                                ; 88 d0                       ; 0xc2e88
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2e8a
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2e8c
    sal ax, CL                                ; d3 e0                       ; 0xc2e8e
    add bx, ax                                ; 01 c3                       ; 0xc2e90
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2e92
    xor bx, bx                                ; 31 db                       ; 0xc2e95 vgabios.c:2009
    jmp short 02e9fh                          ; eb 06                       ; 0xc2e97
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2e99
    jnc short 02ecah                          ; 73 2b                       ; 0xc2e9d
    mov ax, bx                                ; 89 d8                       ; 0xc2e9f vgabios.c:2011
    mov si, strict word 0000eh                ; be 0e 00                    ; 0xc2ea1
    mul si                                    ; f7 e6                       ; 0xc2ea4
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2ea6 vgabios.c:2012
    mov di, bx                                ; 89 df                       ; 0xc2ea8
    sal di, CL                                ; d3 e7                       ; 0xc2eaa
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2eac
    mov si, 05d6ch                            ; be 6c 5d                    ; 0xc2eaf vgabios.c:2013
    add si, ax                                ; 01 c6                       ; 0xc2eb2
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2eb4
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2eb7
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2eba
    mov es, ax                                ; 8e c0                       ; 0xc2ebd
    jcxz 02ec7h                               ; e3 06                       ; 0xc2ebf
    push DS                                   ; 1e                          ; 0xc2ec1
    mov ds, dx                                ; 8e da                       ; 0xc2ec2
    rep movsb                                 ; f3 a4                       ; 0xc2ec4
    pop DS                                    ; 1f                          ; 0xc2ec6
    inc bx                                    ; 43                          ; 0xc2ec7 vgabios.c:2014
    jmp short 02e99h                          ; eb cf                       ; 0xc2ec8
    call 02cd7h                               ; e8 0a fe                    ; 0xc2eca vgabios.c:2015
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2ecd vgabios.c:2016
    jc short 02ed9h                           ; 72 06                       ; 0xc2ed1
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2ed3 vgabios.c:2018
    call 02d16h                               ; e8 3d fe                    ; 0xc2ed6
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2ed9 vgabios.c:2020
    pop di                                    ; 5f                          ; 0xc2edc
    pop si                                    ; 5e                          ; 0xc2edd
    pop cx                                    ; 59                          ; 0xc2ede
    pop bx                                    ; 5b                          ; 0xc2edf
    pop bp                                    ; 5d                          ; 0xc2ee0
    retn                                      ; c3                          ; 0xc2ee1
  ; disGetNextSymbol 0xc2ee2 LB 0x172b -> off=0x0 cb=0000000000000073 uValue=00000000000c2ee2 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2ee2 LB 0x73
    push bp                                   ; 55                          ; 0xc2ee2 vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2ee3
    push bx                                   ; 53                          ; 0xc2ee5
    push cx                                   ; 51                          ; 0xc2ee6
    push si                                   ; 56                          ; 0xc2ee7
    push di                                   ; 57                          ; 0xc2ee8
    push ax                                   ; 50                          ; 0xc2ee9
    push ax                                   ; 50                          ; 0xc2eea
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2eeb
    call 02cabh                               ; e8 ba fd                    ; 0xc2eee vgabios.c:2026
    mov al, dl                                ; 88 d0                       ; 0xc2ef1 vgabios.c:2027
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2ef3
    xor ah, ah                                ; 30 e4                       ; 0xc2ef5
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2ef7
    mov bx, ax                                ; 89 c3                       ; 0xc2ef9
    sal bx, CL                                ; d3 e3                       ; 0xc2efb
    mov al, dl                                ; 88 d0                       ; 0xc2efd
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2eff
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2f01
    sal ax, CL                                ; d3 e0                       ; 0xc2f03
    add bx, ax                                ; 01 c3                       ; 0xc2f05
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2f07
    xor bx, bx                                ; 31 db                       ; 0xc2f0a vgabios.c:2028
    jmp short 02f14h                          ; eb 06                       ; 0xc2f0c
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2f0e
    jnc short 02f3dh                          ; 73 29                       ; 0xc2f12
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2f14 vgabios.c:2030
    mov si, bx                                ; 89 de                       ; 0xc2f16
    sal si, CL                                ; d3 e6                       ; 0xc2f18
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2f1a vgabios.c:2031
    mov di, bx                                ; 89 df                       ; 0xc2f1c
    sal di, CL                                ; d3 e7                       ; 0xc2f1e
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2f20
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2f23 vgabios.c:2032
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2f27
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2f2a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2f2d
    mov es, ax                                ; 8e c0                       ; 0xc2f30
    jcxz 02f3ah                               ; e3 06                       ; 0xc2f32
    push DS                                   ; 1e                          ; 0xc2f34
    mov ds, dx                                ; 8e da                       ; 0xc2f35
    rep movsb                                 ; f3 a4                       ; 0xc2f37
    pop DS                                    ; 1f                          ; 0xc2f39
    inc bx                                    ; 43                          ; 0xc2f3a vgabios.c:2033
    jmp short 02f0eh                          ; eb d1                       ; 0xc2f3b
    call 02cd7h                               ; e8 97 fd                    ; 0xc2f3d vgabios.c:2034
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2f40 vgabios.c:2035
    jc short 02f4ch                           ; 72 06                       ; 0xc2f44
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2f46 vgabios.c:2037
    call 02d16h                               ; e8 ca fd                    ; 0xc2f49
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2f4c vgabios.c:2039
    pop di                                    ; 5f                          ; 0xc2f4f
    pop si                                    ; 5e                          ; 0xc2f50
    pop cx                                    ; 59                          ; 0xc2f51
    pop bx                                    ; 5b                          ; 0xc2f52
    pop bp                                    ; 5d                          ; 0xc2f53
    retn                                      ; c3                          ; 0xc2f54
  ; disGetNextSymbol 0xc2f55 LB 0x16b8 -> off=0x0 cb=0000000000000073 uValue=00000000000c2f55 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2f55 LB 0x73
    push bp                                   ; 55                          ; 0xc2f55 vgabios.c:2042
    mov bp, sp                                ; 89 e5                       ; 0xc2f56
    push bx                                   ; 53                          ; 0xc2f58
    push cx                                   ; 51                          ; 0xc2f59
    push si                                   ; 56                          ; 0xc2f5a
    push di                                   ; 57                          ; 0xc2f5b
    push ax                                   ; 50                          ; 0xc2f5c
    push ax                                   ; 50                          ; 0xc2f5d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2f5e
    call 02cabh                               ; e8 47 fd                    ; 0xc2f61 vgabios.c:2046
    mov al, dl                                ; 88 d0                       ; 0xc2f64 vgabios.c:2047
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2f66
    xor ah, ah                                ; 30 e4                       ; 0xc2f68
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2f6a
    mov bx, ax                                ; 89 c3                       ; 0xc2f6c
    sal bx, CL                                ; d3 e3                       ; 0xc2f6e
    mov al, dl                                ; 88 d0                       ; 0xc2f70
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2f72
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2f74
    sal ax, CL                                ; d3 e0                       ; 0xc2f76
    add bx, ax                                ; 01 c3                       ; 0xc2f78
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2f7a
    xor bx, bx                                ; 31 db                       ; 0xc2f7d vgabios.c:2048
    jmp short 02f87h                          ; eb 06                       ; 0xc2f7f
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2f81
    jnc short 02fb0h                          ; 73 29                       ; 0xc2f85
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc2f87 vgabios.c:2050
    mov si, bx                                ; 89 de                       ; 0xc2f89
    sal si, CL                                ; d3 e6                       ; 0xc2f8b
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2f8d vgabios.c:2051
    mov di, bx                                ; 89 df                       ; 0xc2f8f
    sal di, CL                                ; d3 e7                       ; 0xc2f91
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2f93
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2f96 vgabios.c:2052
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2f9a
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2f9d
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2fa0
    mov es, ax                                ; 8e c0                       ; 0xc2fa3
    jcxz 02fadh                               ; e3 06                       ; 0xc2fa5
    push DS                                   ; 1e                          ; 0xc2fa7
    mov ds, dx                                ; 8e da                       ; 0xc2fa8
    rep movsb                                 ; f3 a4                       ; 0xc2faa
    pop DS                                    ; 1f                          ; 0xc2fac
    inc bx                                    ; 43                          ; 0xc2fad vgabios.c:2053
    jmp short 02f81h                          ; eb d1                       ; 0xc2fae
    call 02cd7h                               ; e8 24 fd                    ; 0xc2fb0 vgabios.c:2054
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2fb3 vgabios.c:2055
    jc short 02fbfh                           ; 72 06                       ; 0xc2fb7
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2fb9 vgabios.c:2057
    call 02d16h                               ; e8 57 fd                    ; 0xc2fbc
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2fbf vgabios.c:2059
    pop di                                    ; 5f                          ; 0xc2fc2
    pop si                                    ; 5e                          ; 0xc2fc3
    pop cx                                    ; 59                          ; 0xc2fc4
    pop bx                                    ; 5b                          ; 0xc2fc5
    pop bp                                    ; 5d                          ; 0xc2fc6
    retn                                      ; c3                          ; 0xc2fc7
  ; disGetNextSymbol 0xc2fc8 LB 0x1645 -> off=0x0 cb=0000000000000016 uValue=00000000000c2fc8 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2fc8 LB 0x16
    push bp                                   ; 55                          ; 0xc2fc8 vgabios.c:2061
    mov bp, sp                                ; 89 e5                       ; 0xc2fc9
    push bx                                   ; 53                          ; 0xc2fcb
    push cx                                   ; 51                          ; 0xc2fcc
    mov bx, dx                                ; 89 d3                       ; 0xc2fcd vgabios.c:2063
    mov cx, ax                                ; 89 c1                       ; 0xc2fcf
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc2fd1
    call 009f0h                               ; e8 19 da                    ; 0xc2fd4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2fd7 vgabios.c:2064
    pop cx                                    ; 59                          ; 0xc2fda
    pop bx                                    ; 5b                          ; 0xc2fdb
    pop bp                                    ; 5d                          ; 0xc2fdc
    retn                                      ; c3                          ; 0xc2fdd
  ; disGetNextSymbol 0xc2fde LB 0x162f -> off=0x0 cb=000000000000004d uValue=00000000000c2fde 'set_gfx_font'
set_gfx_font:                                ; 0xc2fde LB 0x4d
    push bp                                   ; 55                          ; 0xc2fde vgabios.c:2066
    mov bp, sp                                ; 89 e5                       ; 0xc2fdf
    push si                                   ; 56                          ; 0xc2fe1
    push di                                   ; 57                          ; 0xc2fe2
    mov si, ax                                ; 89 c6                       ; 0xc2fe3
    mov ax, dx                                ; 89 d0                       ; 0xc2fe5
    mov di, bx                                ; 89 df                       ; 0xc2fe7
    mov dl, cl                                ; 88 ca                       ; 0xc2fe9
    mov bx, si                                ; 89 f3                       ; 0xc2feb vgabios.c:2070
    mov cx, ax                                ; 89 c1                       ; 0xc2fed
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc2fef
    call 009f0h                               ; e8 fb d9                    ; 0xc2ff2
    test dl, dl                               ; 84 d2                       ; 0xc2ff5 vgabios.c:2071
    je short 0300bh                           ; 74 12                       ; 0xc2ff7
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc2ff9 vgabios.c:2072
    jbe short 03000h                          ; 76 02                       ; 0xc2ffc
    mov DL, strict byte 002h                  ; b2 02                       ; 0xc2ffe vgabios.c:2073
    mov bl, dl                                ; 88 d3                       ; 0xc3000 vgabios.c:2074
    xor bh, bh                                ; 30 ff                       ; 0xc3002
    mov al, byte [bx+07dfdh]                  ; 8a 87 fd 7d                 ; 0xc3004
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc3008
    mov bx, 00085h                            ; bb 85 00                    ; 0xc300b vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc300e
    mov es, ax                                ; 8e c0                       ; 0xc3011
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc3013
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc3016 vgabios.c:2079
    xor ah, ah                                ; 30 e4                       ; 0xc3019
    dec ax                                    ; 48                          ; 0xc301b
    mov bx, 00084h                            ; bb 84 00                    ; 0xc301c vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc301f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3022 vgabios.c:2080
    pop di                                    ; 5f                          ; 0xc3025
    pop si                                    ; 5e                          ; 0xc3026
    pop bp                                    ; 5d                          ; 0xc3027
    retn 00002h                               ; c2 02 00                    ; 0xc3028
  ; disGetNextSymbol 0xc302b LB 0x15e2 -> off=0x0 cb=000000000000001d uValue=00000000000c302b 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc302b LB 0x1d
    push bp                                   ; 55                          ; 0xc302b vgabios.c:2082
    mov bp, sp                                ; 89 e5                       ; 0xc302c
    push si                                   ; 56                          ; 0xc302e
    mov si, ax                                ; 89 c6                       ; 0xc302f
    mov ax, dx                                ; 89 d0                       ; 0xc3031
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc3033 vgabios.c:2085
    xor dh, dh                                ; 30 f6                       ; 0xc3036
    push dx                                   ; 52                          ; 0xc3038
    xor ch, ch                                ; 30 ed                       ; 0xc3039
    mov dx, si                                ; 89 f2                       ; 0xc303b
    call 02fdeh                               ; e8 9e ff                    ; 0xc303d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3040 vgabios.c:2086
    pop si                                    ; 5e                          ; 0xc3043
    pop bp                                    ; 5d                          ; 0xc3044
    retn 00002h                               ; c2 02 00                    ; 0xc3045
  ; disGetNextSymbol 0xc3048 LB 0x15c5 -> off=0x0 cb=0000000000000022 uValue=00000000000c3048 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc3048 LB 0x22
    push bp                                   ; 55                          ; 0xc3048 vgabios.c:2091
    mov bp, sp                                ; 89 e5                       ; 0xc3049
    push bx                                   ; 53                          ; 0xc304b
    push cx                                   ; 51                          ; 0xc304c
    mov bl, al                                ; 88 c3                       ; 0xc304d
    mov al, dl                                ; 88 d0                       ; 0xc304f
    xor ah, ah                                ; 30 e4                       ; 0xc3051 vgabios.c:2093
    push ax                                   ; 50                          ; 0xc3053
    mov al, bl                                ; 88 d8                       ; 0xc3054
    mov cx, ax                                ; 89 c1                       ; 0xc3056
    mov bx, strict word 0000eh                ; bb 0e 00                    ; 0xc3058
    mov ax, 05d6ch                            ; b8 6c 5d                    ; 0xc305b
    mov dx, ds                                ; 8c da                       ; 0xc305e
    call 02fdeh                               ; e8 7b ff                    ; 0xc3060
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3063 vgabios.c:2094
    pop cx                                    ; 59                          ; 0xc3066
    pop bx                                    ; 5b                          ; 0xc3067
    pop bp                                    ; 5d                          ; 0xc3068
    retn                                      ; c3                          ; 0xc3069
  ; disGetNextSymbol 0xc306a LB 0x15a3 -> off=0x0 cb=0000000000000022 uValue=00000000000c306a 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc306a LB 0x22
    push bp                                   ; 55                          ; 0xc306a vgabios.c:2095
    mov bp, sp                                ; 89 e5                       ; 0xc306b
    push bx                                   ; 53                          ; 0xc306d
    push cx                                   ; 51                          ; 0xc306e
    mov bl, al                                ; 88 c3                       ; 0xc306f
    mov al, dl                                ; 88 d0                       ; 0xc3071
    xor ah, ah                                ; 30 e4                       ; 0xc3073 vgabios.c:2097
    push ax                                   ; 50                          ; 0xc3075
    mov al, bl                                ; 88 d8                       ; 0xc3076
    mov cx, ax                                ; 89 c1                       ; 0xc3078
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc307a
    mov ax, 0556ch                            ; b8 6c 55                    ; 0xc307d
    mov dx, ds                                ; 8c da                       ; 0xc3080
    call 02fdeh                               ; e8 59 ff                    ; 0xc3082
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3085 vgabios.c:2098
    pop cx                                    ; 59                          ; 0xc3088
    pop bx                                    ; 5b                          ; 0xc3089
    pop bp                                    ; 5d                          ; 0xc308a
    retn                                      ; c3                          ; 0xc308b
  ; disGetNextSymbol 0xc308c LB 0x1581 -> off=0x0 cb=0000000000000022 uValue=00000000000c308c 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc308c LB 0x22
    push bp                                   ; 55                          ; 0xc308c vgabios.c:2099
    mov bp, sp                                ; 89 e5                       ; 0xc308d
    push bx                                   ; 53                          ; 0xc308f
    push cx                                   ; 51                          ; 0xc3090
    mov bl, al                                ; 88 c3                       ; 0xc3091
    mov al, dl                                ; 88 d0                       ; 0xc3093
    xor ah, ah                                ; 30 e4                       ; 0xc3095 vgabios.c:2101
    push ax                                   ; 50                          ; 0xc3097
    mov al, bl                                ; 88 d8                       ; 0xc3098
    mov cx, ax                                ; 89 c1                       ; 0xc309a
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc309c
    mov ax, 06b6ch                            ; b8 6c 6b                    ; 0xc309f
    mov dx, ds                                ; 8c da                       ; 0xc30a2
    call 02fdeh                               ; e8 37 ff                    ; 0xc30a4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc30a7 vgabios.c:2102
    pop cx                                    ; 59                          ; 0xc30aa
    pop bx                                    ; 5b                          ; 0xc30ab
    pop bp                                    ; 5d                          ; 0xc30ac
    retn                                      ; c3                          ; 0xc30ad
  ; disGetNextSymbol 0xc30ae LB 0x155f -> off=0x0 cb=0000000000000005 uValue=00000000000c30ae 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc30ae LB 0x5
    push bp                                   ; 55                          ; 0xc30ae vgabios.c:2104
    mov bp, sp                                ; 89 e5                       ; 0xc30af
    pop bp                                    ; 5d                          ; 0xc30b1 vgabios.c:2109
    retn                                      ; c3                          ; 0xc30b2
  ; disGetNextSymbol 0xc30b3 LB 0x155a -> off=0x0 cb=0000000000000032 uValue=00000000000c30b3 'biosfn_set_txt_lines'
biosfn_set_txt_lines:                        ; 0xc30b3 LB 0x32
    push bx                                   ; 53                          ; 0xc30b3 vgabios.c:2111
    push si                                   ; 56                          ; 0xc30b4
    push bp                                   ; 55                          ; 0xc30b5
    mov bp, sp                                ; 89 e5                       ; 0xc30b6
    mov bl, al                                ; 88 c3                       ; 0xc30b8
    mov si, 00089h                            ; be 89 00                    ; 0xc30ba vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30bd
    mov es, ax                                ; 8e c0                       ; 0xc30c0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc30c2
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc30c5 vgabios.c:2117
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc30c7 vgabios.c:2119
    je short 030d4h                           ; 74 08                       ; 0xc30ca
    test bl, bl                               ; 84 db                       ; 0xc30cc
    jne short 030d6h                          ; 75 06                       ; 0xc30ce
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc30d0 vgabios.c:2122
    jmp short 030d6h                          ; eb 02                       ; 0xc30d2 vgabios.c:2123
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc30d4 vgabios.c:2125
    mov bx, 00089h                            ; bb 89 00                    ; 0xc30d6 vgabios.c:52
    mov si, strict word 00040h                ; be 40 00                    ; 0xc30d9
    mov es, si                                ; 8e c6                       ; 0xc30dc
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30de
    pop bp                                    ; 5d                          ; 0xc30e1 vgabios.c:2129
    pop si                                    ; 5e                          ; 0xc30e2
    pop bx                                    ; 5b                          ; 0xc30e3
    retn                                      ; c3                          ; 0xc30e4
  ; disGetNextSymbol 0xc30e5 LB 0x1528 -> off=0x0 cb=0000000000000005 uValue=00000000000c30e5 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc30e5 LB 0x5
    push bp                                   ; 55                          ; 0xc30e5 vgabios.c:2132
    mov bp, sp                                ; 89 e5                       ; 0xc30e6
    pop bp                                    ; 5d                          ; 0xc30e8 vgabios.c:2137
    retn                                      ; c3                          ; 0xc30e9
  ; disGetNextSymbol 0xc30ea LB 0x1523 -> off=0x0 cb=0000000000000005 uValue=00000000000c30ea 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc30ea LB 0x5
    push bp                                   ; 55                          ; 0xc30ea vgabios.c:2138
    mov bp, sp                                ; 89 e5                       ; 0xc30eb
    pop bp                                    ; 5d                          ; 0xc30ed vgabios.c:2143
    retn                                      ; c3                          ; 0xc30ee
  ; disGetNextSymbol 0xc30ef LB 0x151e -> off=0x0 cb=000000000000008f uValue=00000000000c30ef 'biosfn_write_string'
biosfn_write_string:                         ; 0xc30ef LB 0x8f
    push bp                                   ; 55                          ; 0xc30ef vgabios.c:2146
    mov bp, sp                                ; 89 e5                       ; 0xc30f0
    push si                                   ; 56                          ; 0xc30f2
    push di                                   ; 57                          ; 0xc30f3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc30f4
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc30f7
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc30fa
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc30fd
    mov si, cx                                ; 89 ce                       ; 0xc3100
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc3102
    mov al, dl                                ; 88 d0                       ; 0xc3105 vgabios.c:2153
    xor ah, ah                                ; 30 e4                       ; 0xc3107
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc3109
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc310c
    call 00a97h                               ; e8 85 d9                    ; 0xc310f
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc3112 vgabios.c:2156
    jne short 03124h                          ; 75 0c                       ; 0xc3116
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3118 vgabios.c:2157
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc311b
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc311e vgabios.c:2158
    mov byte [bp+004h], ah                    ; 88 66 04                    ; 0xc3121
    mov dh, byte [bp+004h]                    ; 8a 76 04                    ; 0xc3124 vgabios.c:2161
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc3127
    xor ah, ah                                ; 30 e4                       ; 0xc312a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc312c vgabios.c:2162
    call 012cfh                               ; e8 9d e1                    ; 0xc312f
    dec si                                    ; 4e                          ; 0xc3132 vgabios.c:2164
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc3133
    je short 03164h                           ; 74 2c                       ; 0xc3136
    mov bx, di                                ; 89 fb                       ; 0xc3138 vgabios.c:2166
    inc di                                    ; 47                          ; 0xc313a
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc313b vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc313e
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc3141 vgabios.c:2167
    je short 03150h                           ; 74 09                       ; 0xc3145
    mov bx, di                                ; 89 fb                       ; 0xc3147 vgabios.c:2168
    inc di                                    ; 47                          ; 0xc3149
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc314a vgabios.c:47
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc314d vgabios.c:48
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc3150 vgabios.c:2170
    xor bh, bh                                ; 30 ff                       ; 0xc3153
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3155
    xor dh, dh                                ; 30 f6                       ; 0xc3158
    xor ah, ah                                ; 30 e4                       ; 0xc315a
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc315c
    call 02a48h                               ; e8 e6 f8                    ; 0xc315f
    jmp short 03132h                          ; eb ce                       ; 0xc3162 vgabios.c:2171
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3164 vgabios.c:2174
    jne short 03175h                          ; 75 0b                       ; 0xc3168
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc316a vgabios.c:2175
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc316d
    xor ah, ah                                ; 30 e4                       ; 0xc3170
    call 012cfh                               ; e8 5a e1                    ; 0xc3172
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3175 vgabios.c:2176
    pop di                                    ; 5f                          ; 0xc3178
    pop si                                    ; 5e                          ; 0xc3179
    pop bp                                    ; 5d                          ; 0xc317a
    retn 00008h                               ; c2 08 00                    ; 0xc317b
  ; disGetNextSymbol 0xc317e LB 0x148f -> off=0x0 cb=00000000000001f2 uValue=00000000000c317e 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc317e LB 0x1f2
    push bp                                   ; 55                          ; 0xc317e vgabios.c:2179
    mov bp, sp                                ; 89 e5                       ; 0xc317f
    push cx                                   ; 51                          ; 0xc3181
    push si                                   ; 56                          ; 0xc3182
    push di                                   ; 57                          ; 0xc3183
    push ax                                   ; 50                          ; 0xc3184
    push ax                                   ; 50                          ; 0xc3185
    push dx                                   ; 52                          ; 0xc3186
    mov si, strict word 00049h                ; be 49 00                    ; 0xc3187 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc318a
    mov es, ax                                ; 8e c0                       ; 0xc318d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc318f
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3192 vgabios.c:48
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3195 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3198
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc319b vgabios.c:58
    mov ax, ds                                ; 8c d8                       ; 0xc319e vgabios.c:2190
    mov es, dx                                ; 8e c2                       ; 0xc31a0 vgabios.c:72
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc31a2
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc31a7
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc31ab vgabios.c:2195
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc31ae
    mov si, strict word 00049h                ; be 49 00                    ; 0xc31b1
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc31b4
    jcxz 031bfh                               ; e3 06                       ; 0xc31b7
    push DS                                   ; 1e                          ; 0xc31b9
    mov ds, dx                                ; 8e da                       ; 0xc31ba
    rep movsb                                 ; f3 a4                       ; 0xc31bc
    pop DS                                    ; 1f                          ; 0xc31be
    mov si, 00084h                            ; be 84 00                    ; 0xc31bf vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31c2
    mov es, ax                                ; 8e c0                       ; 0xc31c5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31c7
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc31ca vgabios.c:48
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc31cc
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc31cf vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc31d2
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc31d5 vgabios.c:2197
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc31d8
    mov si, 00085h                            ; be 85 00                    ; 0xc31db
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc31de
    jcxz 031e9h                               ; e3 06                       ; 0xc31e1
    push DS                                   ; 1e                          ; 0xc31e3
    mov ds, dx                                ; 8e da                       ; 0xc31e4
    rep movsb                                 ; f3 a4                       ; 0xc31e6
    pop DS                                    ; 1f                          ; 0xc31e8
    mov si, 0008ah                            ; be 8a 00                    ; 0xc31e9 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31ec
    mov es, ax                                ; 8e c0                       ; 0xc31ef
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31f1
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc31f4 vgabios.c:48
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc31f7 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc31fa
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc31fd vgabios.c:2200
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc3200 vgabios.c:52
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3204 vgabios.c:2201
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc3207 vgabios.c:62
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc320c vgabios.c:2202
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc320f vgabios.c:52
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc3213 vgabios.c:2203
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc3216 vgabios.c:52
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc321a vgabios.c:2204
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc321d vgabios.c:52
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc3221 vgabios.c:2205
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc3224 vgabios.c:52
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc3228 vgabios.c:2206
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc322b vgabios.c:52
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc322f vgabios.c:2207
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc3232 vgabios.c:52
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc3236 vgabios.c:2208
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc3239 vgabios.c:52
    mov si, 00089h                            ; be 89 00                    ; 0xc323d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3240
    mov es, ax                                ; 8e c0                       ; 0xc3243
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3245
    mov dl, al                                ; 88 c2                       ; 0xc3248 vgabios.c:2213
    and dl, 080h                              ; 80 e2 80                    ; 0xc324a
    xor dh, dh                                ; 30 f6                       ; 0xc324d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc324f
    sar dx, CL                                ; d3 fa                       ; 0xc3251
    and AL, strict byte 010h                  ; 24 10                       ; 0xc3253
    xor ah, ah                                ; 30 e4                       ; 0xc3255
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc3257
    sar ax, CL                                ; d3 f8                       ; 0xc3259
    or ax, dx                                 ; 09 d0                       ; 0xc325b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc325d vgabios.c:2214
    je short 03273h                           ; 74 11                       ; 0xc3260
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3262
    je short 0326fh                           ; 74 08                       ; 0xc3265
    test ax, ax                               ; 85 c0                       ; 0xc3267
    jne short 03273h                          ; 75 08                       ; 0xc3269
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc326b vgabios.c:2215
    jmp short 03275h                          ; eb 06                       ; 0xc326d
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc326f vgabios.c:2216
    jmp short 03275h                          ; eb 02                       ; 0xc3271
    xor al, al                                ; 30 c0                       ; 0xc3273 vgabios.c:2218
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc3275 vgabios.c:2220
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3278 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc327b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc327e vgabios.c:2223
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc3281
    jc short 032a5h                           ; 72 20                       ; 0xc3283
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc3285
    jnbe short 032a5h                         ; 77 1c                       ; 0xc3287
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3289 vgabios.c:2224
    test ax, ax                               ; 85 c0                       ; 0xc328c
    je short 032e7h                           ; 74 57                       ; 0xc328e
    mov si, ax                                ; 89 c6                       ; 0xc3290 vgabios.c:2225
    shr si, 1                                 ; d1 ee                       ; 0xc3292
    shr si, 1                                 ; d1 ee                       ; 0xc3294
    mov ax, 04000h                            ; b8 00 40                    ; 0xc3296
    xor dx, dx                                ; 31 d2                       ; 0xc3299
    div si                                    ; f7 f6                       ; 0xc329b
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc329d
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32a0 vgabios.c:52
    jmp short 032e7h                          ; eb 42                       ; 0xc32a3 vgabios.c:2226
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc32a5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc32a8
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc32ab
    jne short 032c0h                          ; 75 11                       ; 0xc32ad
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32af vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc32b2
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc32b6 vgabios.c:2228
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc32b9 vgabios.c:62
    jmp short 032e7h                          ; eb 27                       ; 0xc32be vgabios.c:2229
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc32c0
    jc short 032e7h                           ; 72 23                       ; 0xc32c2
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc32c4
    jnbe short 032e7h                         ; 77 1f                       ; 0xc32c6
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc32c8 vgabios.c:2231
    je short 032dch                           ; 74 0e                       ; 0xc32cc
    mov ax, 04000h                            ; b8 00 40                    ; 0xc32ce vgabios.c:2232
    xor dx, dx                                ; 31 d2                       ; 0xc32d1
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc32d3
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32d6 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32d9
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc32dc vgabios.c:2233
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32df vgabios.c:62
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc32e2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc32e7 vgabios.c:2235
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc32ea
    je short 032f2h                           ; 74 04                       ; 0xc32ec
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc32ee
    jne short 032fdh                          ; 75 0b                       ; 0xc32f0
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc32f2 vgabios.c:2236
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32f5 vgabios.c:62
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc32f8
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc32fd vgabios.c:2238
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3300
    jc short 03359h                           ; 72 55                       ; 0xc3302
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc3304
    je short 03359h                           ; 74 51                       ; 0xc3306
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc3308 vgabios.c:2239
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc330b vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc330e
    mov si, 00084h                            ; be 84 00                    ; 0xc3312 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3315
    mov es, ax                                ; 8e c0                       ; 0xc3318
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc331a
    xor ah, ah                                ; 30 e4                       ; 0xc331d vgabios.c:48
    inc ax                                    ; 40                          ; 0xc331f
    mov si, 00085h                            ; be 85 00                    ; 0xc3320 vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc3323
    xor dh, dh                                ; 30 f6                       ; 0xc3326 vgabios.c:48
    imul dx                                   ; f7 ea                       ; 0xc3328
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc332a vgabios.c:2241
    jc short 0333dh                           ; 72 0e                       ; 0xc332d
    jbe short 03346h                          ; 76 15                       ; 0xc332f
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc3331
    je short 0334eh                           ; 74 18                       ; 0xc3334
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc3336
    je short 0334ah                           ; 74 0f                       ; 0xc3339
    jmp short 0334eh                          ; eb 11                       ; 0xc333b
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc333d
    jne short 0334eh                          ; 75 0c                       ; 0xc3340
    xor al, al                                ; 30 c0                       ; 0xc3342 vgabios.c:2242
    jmp short 03350h                          ; eb 0a                       ; 0xc3344
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc3346 vgabios.c:2243
    jmp short 03350h                          ; eb 06                       ; 0xc3348
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc334a vgabios.c:2244
    jmp short 03350h                          ; eb 02                       ; 0xc334c
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc334e vgabios.c:2246
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc3350 vgabios.c:2248
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3353 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3356
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc3359 vgabios.c:2251
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc335c
    xor ax, ax                                ; 31 c0                       ; 0xc335f
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3361
    jcxz 03368h                               ; e3 02                       ; 0xc3364
    rep stosb                                 ; f3 aa                       ; 0xc3366
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3368 vgabios.c:2252
    pop di                                    ; 5f                          ; 0xc336b
    pop si                                    ; 5e                          ; 0xc336c
    pop cx                                    ; 59                          ; 0xc336d
    pop bp                                    ; 5d                          ; 0xc336e
    retn                                      ; c3                          ; 0xc336f
  ; disGetNextSymbol 0xc3370 LB 0x129d -> off=0x0 cb=0000000000000023 uValue=00000000000c3370 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc3370 LB 0x23
    push dx                                   ; 52                          ; 0xc3370 vgabios.c:2255
    push bp                                   ; 55                          ; 0xc3371
    mov bp, sp                                ; 89 e5                       ; 0xc3372
    mov dx, ax                                ; 89 c2                       ; 0xc3374
    xor ax, ax                                ; 31 c0                       ; 0xc3376 vgabios.c:2259
    test dl, 001h                             ; f6 c2 01                    ; 0xc3378 vgabios.c:2260
    je short 03380h                           ; 74 03                       ; 0xc337b
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc337d vgabios.c:2261
    test dl, 002h                             ; f6 c2 02                    ; 0xc3380 vgabios.c:2263
    je short 03388h                           ; 74 03                       ; 0xc3383
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc3385 vgabios.c:2264
    test dl, 004h                             ; f6 c2 04                    ; 0xc3388 vgabios.c:2266
    je short 03390h                           ; 74 03                       ; 0xc338b
    add ax, 00304h                            ; 05 04 03                    ; 0xc338d vgabios.c:2267
    pop bp                                    ; 5d                          ; 0xc3390 vgabios.c:2270
    pop dx                                    ; 5a                          ; 0xc3391
    retn                                      ; c3                          ; 0xc3392
  ; disGetNextSymbol 0xc3393 LB 0x127a -> off=0x0 cb=000000000000001b uValue=00000000000c3393 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3393 LB 0x1b
    push bp                                   ; 55                          ; 0xc3393 vgabios.c:2272
    mov bp, sp                                ; 89 e5                       ; 0xc3394
    push bx                                   ; 53                          ; 0xc3396
    push cx                                   ; 51                          ; 0xc3397
    mov bx, dx                                ; 89 d3                       ; 0xc3398
    call 03370h                               ; e8 d3 ff                    ; 0xc339a vgabios.c:2275
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc339d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc33a0
    shr ax, CL                                ; d3 e8                       ; 0xc33a2
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc33a4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc33a7 vgabios.c:2276
    pop cx                                    ; 59                          ; 0xc33aa
    pop bx                                    ; 5b                          ; 0xc33ab
    pop bp                                    ; 5d                          ; 0xc33ac
    retn                                      ; c3                          ; 0xc33ad
  ; disGetNextSymbol 0xc33ae LB 0x125f -> off=0x0 cb=00000000000002d8 uValue=00000000000c33ae 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc33ae LB 0x2d8
    push bp                                   ; 55                          ; 0xc33ae vgabios.c:2278
    mov bp, sp                                ; 89 e5                       ; 0xc33af
    push cx                                   ; 51                          ; 0xc33b1
    push si                                   ; 56                          ; 0xc33b2
    push di                                   ; 57                          ; 0xc33b3
    push ax                                   ; 50                          ; 0xc33b4
    push ax                                   ; 50                          ; 0xc33b5
    push ax                                   ; 50                          ; 0xc33b6
    mov cx, dx                                ; 89 d1                       ; 0xc33b7
    mov si, strict word 00063h                ; be 63 00                    ; 0xc33b9 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc33bc
    mov es, ax                                ; 8e c0                       ; 0xc33bf
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc33c1
    mov si, di                                ; 89 fe                       ; 0xc33c4 vgabios.c:58
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc33c6 vgabios.c:2283
    je short 03432h                           ; 74 66                       ; 0xc33ca
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33cc vgabios.c:2284
    in AL, DX                                 ; ec                          ; 0xc33cf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33d0
    mov es, cx                                ; 8e c1                       ; 0xc33d2 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33d4
    inc bx                                    ; 43                          ; 0xc33d7 vgabios.c:2284
    mov dx, di                                ; 89 fa                       ; 0xc33d8
    in AL, DX                                 ; ec                          ; 0xc33da
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33db
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33dd vgabios.c:52
    inc bx                                    ; 43                          ; 0xc33e0 vgabios.c:2285
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc33e1
    in AL, DX                                 ; ec                          ; 0xc33e4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33e5
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33e7 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc33ea vgabios.c:2286
    mov dx, 003dah                            ; ba da 03                    ; 0xc33eb
    in AL, DX                                 ; ec                          ; 0xc33ee
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33ef
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc33f1 vgabios.c:2288
    in AL, DX                                 ; ec                          ; 0xc33f4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33f5
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc33f7
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc33fa vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33fd
    inc bx                                    ; 43                          ; 0xc3400 vgabios.c:2289
    mov dx, 003cah                            ; ba ca 03                    ; 0xc3401
    in AL, DX                                 ; ec                          ; 0xc3404
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3405
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3407 vgabios.c:52
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc340a vgabios.c:2292
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc340d
    add bx, ax                                ; 01 c3                       ; 0xc3410 vgabios.c:2290
    jmp short 0341ah                          ; eb 06                       ; 0xc3412
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3414
    jnbe short 03435h                         ; 77 1b                       ; 0xc3418
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc341a vgabios.c:2293
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc341d
    out DX, AL                                ; ee                          ; 0xc3420
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3421 vgabios.c:2294
    in AL, DX                                 ; ec                          ; 0xc3424
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3425
    mov es, cx                                ; 8e c1                       ; 0xc3427 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3429
    inc bx                                    ; 43                          ; 0xc342c vgabios.c:2294
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc342d vgabios.c:2295
    jmp short 03414h                          ; eb e2                       ; 0xc3430
    jmp near 034e2h                           ; e9 ad 00                    ; 0xc3432
    xor al, al                                ; 30 c0                       ; 0xc3435 vgabios.c:2296
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3437
    out DX, AL                                ; ee                          ; 0xc343a
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc343b vgabios.c:2297
    in AL, DX                                 ; ec                          ; 0xc343e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc343f
    mov es, cx                                ; 8e c1                       ; 0xc3441 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3443
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3446 vgabios.c:2299
    inc bx                                    ; 43                          ; 0xc344b vgabios.c:2297
    jmp short 03454h                          ; eb 06                       ; 0xc344c
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc344e
    jnbe short 0346bh                         ; 77 17                       ; 0xc3452
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3454 vgabios.c:2300
    mov dx, si                                ; 89 f2                       ; 0xc3457
    out DX, AL                                ; ee                          ; 0xc3459
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc345a vgabios.c:2301
    in AL, DX                                 ; ec                          ; 0xc345d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc345e
    mov es, cx                                ; 8e c1                       ; 0xc3460 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3462
    inc bx                                    ; 43                          ; 0xc3465 vgabios.c:2301
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3466 vgabios.c:2302
    jmp short 0344eh                          ; eb e3                       ; 0xc3469
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc346b vgabios.c:2304
    jmp short 03478h                          ; eb 06                       ; 0xc3470
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3472
    jnbe short 0349ch                         ; 77 24                       ; 0xc3476
    mov dx, 003dah                            ; ba da 03                    ; 0xc3478 vgabios.c:2305
    in AL, DX                                 ; ec                          ; 0xc347b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc347c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc347e vgabios.c:2306
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3481
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3484
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3487
    out DX, AL                                ; ee                          ; 0xc348a
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc348b vgabios.c:2307
    in AL, DX                                 ; ec                          ; 0xc348e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc348f
    mov es, cx                                ; 8e c1                       ; 0xc3491 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3493
    inc bx                                    ; 43                          ; 0xc3496 vgabios.c:2307
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3497 vgabios.c:2308
    jmp short 03472h                          ; eb d6                       ; 0xc349a
    mov dx, 003dah                            ; ba da 03                    ; 0xc349c vgabios.c:2309
    in AL, DX                                 ; ec                          ; 0xc349f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc34a0
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc34a2 vgabios.c:2311
    jmp short 034afh                          ; eb 06                       ; 0xc34a7
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc34a9
    jnbe short 034c7h                         ; 77 18                       ; 0xc34ad
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc34af vgabios.c:2312
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc34b2
    out DX, AL                                ; ee                          ; 0xc34b5
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc34b6 vgabios.c:2313
    in AL, DX                                 ; ec                          ; 0xc34b9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc34ba
    mov es, cx                                ; 8e c1                       ; 0xc34bc vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc34be
    inc bx                                    ; 43                          ; 0xc34c1 vgabios.c:2313
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc34c2 vgabios.c:2314
    jmp short 034a9h                          ; eb e2                       ; 0xc34c5
    mov es, cx                                ; 8e c1                       ; 0xc34c7 vgabios.c:62
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc34c9
    inc bx                                    ; 43                          ; 0xc34cc vgabios.c:2316
    inc bx                                    ; 43                          ; 0xc34cd
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc34ce vgabios.c:52
    inc bx                                    ; 43                          ; 0xc34d2 vgabios.c:2319
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc34d3 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc34d7 vgabios.c:2320
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc34d8 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc34dc vgabios.c:2321
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc34dd vgabios.c:52
    inc bx                                    ; 43                          ; 0xc34e1 vgabios.c:2322
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc34e2 vgabios.c:2324
    jne short 034ebh                          ; 75 03                       ; 0xc34e6
    jmp near 0362ah                           ; e9 3f 01                    ; 0xc34e8
    mov si, strict word 00049h                ; be 49 00                    ; 0xc34eb vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34ee
    mov es, ax                                ; 8e c0                       ; 0xc34f1
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34f3
    mov es, cx                                ; 8e c1                       ; 0xc34f6 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc34f8
    inc bx                                    ; 43                          ; 0xc34fb vgabios.c:2325
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc34fc vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34ff
    mov es, ax                                ; 8e c0                       ; 0xc3502
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3504
    mov es, cx                                ; 8e c1                       ; 0xc3507 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3509
    inc bx                                    ; 43                          ; 0xc350c vgabios.c:2326
    inc bx                                    ; 43                          ; 0xc350d
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc350e vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3511
    mov es, ax                                ; 8e c0                       ; 0xc3514
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3516
    mov es, cx                                ; 8e c1                       ; 0xc3519 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc351b
    inc bx                                    ; 43                          ; 0xc351e vgabios.c:2327
    inc bx                                    ; 43                          ; 0xc351f
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3520 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3523
    mov es, ax                                ; 8e c0                       ; 0xc3526
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3528
    mov es, cx                                ; 8e c1                       ; 0xc352b vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc352d
    inc bx                                    ; 43                          ; 0xc3530 vgabios.c:2328
    inc bx                                    ; 43                          ; 0xc3531
    mov si, 00084h                            ; be 84 00                    ; 0xc3532 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3535
    mov es, ax                                ; 8e c0                       ; 0xc3538
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc353a
    mov es, cx                                ; 8e c1                       ; 0xc353d vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc353f
    inc bx                                    ; 43                          ; 0xc3542 vgabios.c:2329
    mov si, 00085h                            ; be 85 00                    ; 0xc3543 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3546
    mov es, ax                                ; 8e c0                       ; 0xc3549
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc354b
    mov es, cx                                ; 8e c1                       ; 0xc354e vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3550
    inc bx                                    ; 43                          ; 0xc3553 vgabios.c:2330
    inc bx                                    ; 43                          ; 0xc3554
    mov si, 00087h                            ; be 87 00                    ; 0xc3555 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3558
    mov es, ax                                ; 8e c0                       ; 0xc355b
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc355d
    mov es, cx                                ; 8e c1                       ; 0xc3560 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3562
    inc bx                                    ; 43                          ; 0xc3565 vgabios.c:2331
    mov si, 00088h                            ; be 88 00                    ; 0xc3566 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3569
    mov es, ax                                ; 8e c0                       ; 0xc356c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc356e
    mov es, cx                                ; 8e c1                       ; 0xc3571 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3573
    inc bx                                    ; 43                          ; 0xc3576 vgabios.c:2332
    mov si, 00089h                            ; be 89 00                    ; 0xc3577 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc357a
    mov es, ax                                ; 8e c0                       ; 0xc357d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc357f
    mov es, cx                                ; 8e c1                       ; 0xc3582 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3584
    inc bx                                    ; 43                          ; 0xc3587 vgabios.c:2333
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3588 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc358b
    mov es, ax                                ; 8e c0                       ; 0xc358e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3590
    mov es, cx                                ; 8e c1                       ; 0xc3593 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3595
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3598 vgabios.c:2335
    inc bx                                    ; 43                          ; 0xc359d vgabios.c:2334
    inc bx                                    ; 43                          ; 0xc359e
    jmp short 035a7h                          ; eb 06                       ; 0xc359f
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc35a1
    jnc short 035c3h                          ; 73 1c                       ; 0xc35a5
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc35a7 vgabios.c:2336
    sal si, 1                                 ; d1 e6                       ; 0xc35aa
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc35ac
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc35af vgabios.c:57
    mov es, ax                                ; 8e c0                       ; 0xc35b2
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc35b4
    mov es, cx                                ; 8e c1                       ; 0xc35b7 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc35b9
    inc bx                                    ; 43                          ; 0xc35bc vgabios.c:2337
    inc bx                                    ; 43                          ; 0xc35bd
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc35be vgabios.c:2338
    jmp short 035a1h                          ; eb de                       ; 0xc35c1
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc35c3 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc35c6
    mov es, ax                                ; 8e c0                       ; 0xc35c9
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc35cb
    mov es, cx                                ; 8e c1                       ; 0xc35ce vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc35d0
    inc bx                                    ; 43                          ; 0xc35d3 vgabios.c:2339
    inc bx                                    ; 43                          ; 0xc35d4
    mov si, strict word 00062h                ; be 62 00                    ; 0xc35d5 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc35d8
    mov es, ax                                ; 8e c0                       ; 0xc35db
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc35dd
    mov es, cx                                ; 8e c1                       ; 0xc35e0 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc35e2
    inc bx                                    ; 43                          ; 0xc35e5 vgabios.c:2340
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc35e6 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc35e9
    mov es, ax                                ; 8e c0                       ; 0xc35eb
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc35ed
    mov es, cx                                ; 8e c1                       ; 0xc35f0 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc35f2
    inc bx                                    ; 43                          ; 0xc35f5 vgabios.c:2342
    inc bx                                    ; 43                          ; 0xc35f6
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc35f7 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc35fa
    mov es, ax                                ; 8e c0                       ; 0xc35fc
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc35fe
    mov es, cx                                ; 8e c1                       ; 0xc3601 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3603
    inc bx                                    ; 43                          ; 0xc3606 vgabios.c:2343
    inc bx                                    ; 43                          ; 0xc3607
    mov si, 0010ch                            ; be 0c 01                    ; 0xc3608 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc360b
    mov es, ax                                ; 8e c0                       ; 0xc360d
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc360f
    mov es, cx                                ; 8e c1                       ; 0xc3612 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3614
    inc bx                                    ; 43                          ; 0xc3617 vgabios.c:2344
    inc bx                                    ; 43                          ; 0xc3618
    mov si, 0010eh                            ; be 0e 01                    ; 0xc3619 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc361c
    mov es, ax                                ; 8e c0                       ; 0xc361e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3620
    mov es, cx                                ; 8e c1                       ; 0xc3623 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3625
    inc bx                                    ; 43                          ; 0xc3628 vgabios.c:2345
    inc bx                                    ; 43                          ; 0xc3629
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc362a vgabios.c:2347
    je short 0367ch                           ; 74 4c                       ; 0xc362e
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc3630 vgabios.c:2349
    in AL, DX                                 ; ec                          ; 0xc3633
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3634
    mov es, cx                                ; 8e c1                       ; 0xc3636 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3638
    inc bx                                    ; 43                          ; 0xc363b vgabios.c:2349
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc363c
    in AL, DX                                 ; ec                          ; 0xc363f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3640
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3642 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3645 vgabios.c:2350
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3646
    in AL, DX                                 ; ec                          ; 0xc3649
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc364a
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc364c vgabios.c:52
    inc bx                                    ; 43                          ; 0xc364f vgabios.c:2351
    xor al, al                                ; 30 c0                       ; 0xc3650
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3652
    out DX, AL                                ; ee                          ; 0xc3655
    xor ah, ah                                ; 30 e4                       ; 0xc3656 vgabios.c:2354
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3658
    jmp short 03664h                          ; eb 07                       ; 0xc365b
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc365d
    jnc short 03675h                          ; 73 11                       ; 0xc3662
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3664 vgabios.c:2355
    in AL, DX                                 ; ec                          ; 0xc3667
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3668
    mov es, cx                                ; 8e c1                       ; 0xc366a vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc366c
    inc bx                                    ; 43                          ; 0xc366f vgabios.c:2355
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3670 vgabios.c:2356
    jmp short 0365dh                          ; eb e8                       ; 0xc3673
    mov es, cx                                ; 8e c1                       ; 0xc3675 vgabios.c:52
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3677
    inc bx                                    ; 43                          ; 0xc367b vgabios.c:2357
    mov ax, bx                                ; 89 d8                       ; 0xc367c vgabios.c:2360
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc367e
    pop di                                    ; 5f                          ; 0xc3681
    pop si                                    ; 5e                          ; 0xc3682
    pop cx                                    ; 59                          ; 0xc3683
    pop bp                                    ; 5d                          ; 0xc3684
    retn                                      ; c3                          ; 0xc3685
  ; disGetNextSymbol 0xc3686 LB 0xf87 -> off=0x0 cb=00000000000002ba uValue=00000000000c3686 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3686 LB 0x2ba
    push bp                                   ; 55                          ; 0xc3686 vgabios.c:2362
    mov bp, sp                                ; 89 e5                       ; 0xc3687
    push cx                                   ; 51                          ; 0xc3689
    push si                                   ; 56                          ; 0xc368a
    push di                                   ; 57                          ; 0xc368b
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc368c
    push ax                                   ; 50                          ; 0xc368f
    mov cx, dx                                ; 89 d1                       ; 0xc3690
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3692 vgabios.c:2366
    je short 0370ch                           ; 74 74                       ; 0xc3696
    mov dx, 003dah                            ; ba da 03                    ; 0xc3698 vgabios.c:2368
    in AL, DX                                 ; ec                          ; 0xc369b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc369c
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc369e vgabios.c:2370
    mov es, cx                                ; 8e c1                       ; 0xc36a1 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc36a3
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc36a6 vgabios.c:58
    mov si, bx                                ; 89 de                       ; 0xc36a9 vgabios.c:2371
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc36ab vgabios.c:2374
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc36b0 vgabios.c:2372
    jmp short 036bbh                          ; eb 06                       ; 0xc36b3
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc36b5
    jnbe short 036d1h                         ; 77 16                       ; 0xc36b9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc36bb vgabios.c:2375
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc36be
    out DX, AL                                ; ee                          ; 0xc36c1
    mov es, cx                                ; 8e c1                       ; 0xc36c2 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc36c4
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc36c7 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc36ca
    inc bx                                    ; 43                          ; 0xc36cb vgabios.c:2376
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc36cc vgabios.c:2377
    jmp short 036b5h                          ; eb e4                       ; 0xc36cf
    xor al, al                                ; 30 c0                       ; 0xc36d1 vgabios.c:2378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc36d3
    out DX, AL                                ; ee                          ; 0xc36d6
    mov es, cx                                ; 8e c1                       ; 0xc36d7 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc36d9
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc36dc vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc36df
    inc bx                                    ; 43                          ; 0xc36e0 vgabios.c:2379
    mov dx, 003cch                            ; ba cc 03                    ; 0xc36e1
    in AL, DX                                 ; ec                          ; 0xc36e4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc36e5
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc36e7
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc36e9
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc36ec vgabios.c:2383
    jne short 036f7h                          ; 75 04                       ; 0xc36f1
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc36f3 vgabios.c:2384
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc36f7 vgabios.c:2385
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc36fa
    out DX, AL                                ; ee                          ; 0xc36fd
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc36fe vgabios.c:2388
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3701
    out DX, ax                                ; ef                          ; 0xc3704
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3705 vgabios.c:2390
    jmp short 03715h                          ; eb 09                       ; 0xc370a
    jmp near 037cfh                           ; e9 c0 00                    ; 0xc370c
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc370f
    jnbe short 0372fh                         ; 77 1a                       ; 0xc3713
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc3715 vgabios.c:2391
    je short 03729h                           ; 74 0e                       ; 0xc3719
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc371b vgabios.c:2392
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc371e
    out DX, AL                                ; ee                          ; 0xc3721
    mov es, cx                                ; 8e c1                       ; 0xc3722 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3724
    inc dx                                    ; 42                          ; 0xc3727 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3728
    inc bx                                    ; 43                          ; 0xc3729 vgabios.c:2395
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc372a vgabios.c:2396
    jmp short 0370fh                          ; eb e0                       ; 0xc372d
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc372f vgabios.c:2398
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3731
    out DX, AL                                ; ee                          ; 0xc3734
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc3735 vgabios.c:2399
    mov es, cx                                ; 8e c1                       ; 0xc3739 vgabios.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc373b
    inc dx                                    ; 42                          ; 0xc373e vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc373f
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc3740 vgabios.c:2402
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc3743 vgabios.c:47
    xor dh, dh                                ; 30 f6                       ; 0xc3746 vgabios.c:48
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3748
    mov dx, 003dah                            ; ba da 03                    ; 0xc374b vgabios.c:2403
    in AL, DX                                 ; ec                          ; 0xc374e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc374f
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3751 vgabios.c:2404
    jmp short 0375eh                          ; eb 06                       ; 0xc3756
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3758
    jnbe short 03777h                         ; 77 19                       ; 0xc375c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc375e vgabios.c:2405
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3761
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3764
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3767
    out DX, AL                                ; ee                          ; 0xc376a
    mov es, cx                                ; 8e c1                       ; 0xc376b vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc376d
    out DX, AL                                ; ee                          ; 0xc3770 vgabios.c:48
    inc bx                                    ; 43                          ; 0xc3771 vgabios.c:2406
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3772 vgabios.c:2407
    jmp short 03758h                          ; eb e1                       ; 0xc3775
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3777 vgabios.c:2408
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc377a
    out DX, AL                                ; ee                          ; 0xc377d
    mov dx, 003dah                            ; ba da 03                    ; 0xc377e vgabios.c:2409
    in AL, DX                                 ; ec                          ; 0xc3781
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3782
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3784 vgabios.c:2411
    jmp short 03791h                          ; eb 06                       ; 0xc3789
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc378b
    jnbe short 037a7h                         ; 77 16                       ; 0xc378f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3791 vgabios.c:2412
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3794
    out DX, AL                                ; ee                          ; 0xc3797
    mov es, cx                                ; 8e c1                       ; 0xc3798 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc379a
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc379d vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc37a0
    inc bx                                    ; 43                          ; 0xc37a1 vgabios.c:2413
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc37a2 vgabios.c:2414
    jmp short 0378bh                          ; eb e4                       ; 0xc37a5
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc37a7 vgabios.c:2415
    mov es, cx                                ; 8e c1                       ; 0xc37aa vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc37ac
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc37af vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc37b2
    inc si                                    ; 46                          ; 0xc37b3 vgabios.c:2418
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc37b4 vgabios.c:47
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc37b7 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc37ba
    inc si                                    ; 46                          ; 0xc37bb vgabios.c:2419
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc37bc vgabios.c:47
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc37bf vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc37c2
    inc si                                    ; 46                          ; 0xc37c3 vgabios.c:2420
    inc si                                    ; 46                          ; 0xc37c4
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc37c5 vgabios.c:47
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc37c8 vgabios.c:48
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc37cb
    out DX, AL                                ; ee                          ; 0xc37ce
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc37cf vgabios.c:2424
    jne short 037d8h                          ; 75 03                       ; 0xc37d3
    jmp near 038f3h                           ; e9 1b 01                    ; 0xc37d5
    mov es, cx                                ; 8e c1                       ; 0xc37d8 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc37da
    mov si, strict word 00049h                ; be 49 00                    ; 0xc37dd vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc37e0
    mov es, dx                                ; 8e c2                       ; 0xc37e3
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc37e5
    inc bx                                    ; 43                          ; 0xc37e8 vgabios.c:2425
    mov es, cx                                ; 8e c1                       ; 0xc37e9 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc37eb
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc37ee vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc37f1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc37f3
    inc bx                                    ; 43                          ; 0xc37f6 vgabios.c:2426
    inc bx                                    ; 43                          ; 0xc37f7
    mov es, cx                                ; 8e c1                       ; 0xc37f8 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc37fa
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc37fd vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3800
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3802
    inc bx                                    ; 43                          ; 0xc3805 vgabios.c:2427
    inc bx                                    ; 43                          ; 0xc3806
    mov es, cx                                ; 8e c1                       ; 0xc3807 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3809
    mov si, strict word 00063h                ; be 63 00                    ; 0xc380c vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc380f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3811
    inc bx                                    ; 43                          ; 0xc3814 vgabios.c:2428
    inc bx                                    ; 43                          ; 0xc3815
    mov es, cx                                ; 8e c1                       ; 0xc3816 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3818
    mov si, 00084h                            ; be 84 00                    ; 0xc381b vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc381e
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3820
    inc bx                                    ; 43                          ; 0xc3823 vgabios.c:2429
    mov es, cx                                ; 8e c1                       ; 0xc3824 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3826
    mov si, 00085h                            ; be 85 00                    ; 0xc3829 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc382c
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc382e
    inc bx                                    ; 43                          ; 0xc3831 vgabios.c:2430
    inc bx                                    ; 43                          ; 0xc3832
    mov es, cx                                ; 8e c1                       ; 0xc3833 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3835
    mov si, 00087h                            ; be 87 00                    ; 0xc3838 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc383b
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc383d
    inc bx                                    ; 43                          ; 0xc3840 vgabios.c:2431
    mov es, cx                                ; 8e c1                       ; 0xc3841 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3843
    mov si, 00088h                            ; be 88 00                    ; 0xc3846 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3849
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc384b
    inc bx                                    ; 43                          ; 0xc384e vgabios.c:2432
    mov es, cx                                ; 8e c1                       ; 0xc384f vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3851
    mov si, 00089h                            ; be 89 00                    ; 0xc3854 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3857
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3859
    inc bx                                    ; 43                          ; 0xc385c vgabios.c:2433
    mov es, cx                                ; 8e c1                       ; 0xc385d vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc385f
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3862 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3865
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3867
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc386a vgabios.c:2435
    inc bx                                    ; 43                          ; 0xc386f vgabios.c:2434
    inc bx                                    ; 43                          ; 0xc3870
    jmp short 03879h                          ; eb 06                       ; 0xc3871
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3873
    jnc short 03895h                          ; 73 1c                       ; 0xc3877
    mov es, cx                                ; 8e c1                       ; 0xc3879 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc387b
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc387e vgabios.c:58
    sal si, 1                                 ; d1 e6                       ; 0xc3881
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3883
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3886 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3889
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc388b
    inc bx                                    ; 43                          ; 0xc388e vgabios.c:2437
    inc bx                                    ; 43                          ; 0xc388f
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3890 vgabios.c:2438
    jmp short 03873h                          ; eb de                       ; 0xc3893
    mov es, cx                                ; 8e c1                       ; 0xc3895 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3897
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc389a vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc389d
    mov es, dx                                ; 8e c2                       ; 0xc38a0
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc38a2
    inc bx                                    ; 43                          ; 0xc38a5 vgabios.c:2439
    inc bx                                    ; 43                          ; 0xc38a6
    mov es, cx                                ; 8e c1                       ; 0xc38a7 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc38a9
    mov si, strict word 00062h                ; be 62 00                    ; 0xc38ac vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc38af
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc38b1
    inc bx                                    ; 43                          ; 0xc38b4 vgabios.c:2440
    mov es, cx                                ; 8e c1                       ; 0xc38b5 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc38b7
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc38ba vgabios.c:62
    xor dx, dx                                ; 31 d2                       ; 0xc38bd
    mov es, dx                                ; 8e c2                       ; 0xc38bf
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc38c1
    inc bx                                    ; 43                          ; 0xc38c4 vgabios.c:2442
    inc bx                                    ; 43                          ; 0xc38c5
    mov es, cx                                ; 8e c1                       ; 0xc38c6 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc38c8
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc38cb vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc38ce
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc38d0
    inc bx                                    ; 43                          ; 0xc38d3 vgabios.c:2443
    inc bx                                    ; 43                          ; 0xc38d4
    mov es, cx                                ; 8e c1                       ; 0xc38d5 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc38d7
    mov si, 0010ch                            ; be 0c 01                    ; 0xc38da vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc38dd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc38df
    inc bx                                    ; 43                          ; 0xc38e2 vgabios.c:2444
    inc bx                                    ; 43                          ; 0xc38e3
    mov es, cx                                ; 8e c1                       ; 0xc38e4 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc38e6
    mov si, 0010eh                            ; be 0e 01                    ; 0xc38e9 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc38ec
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc38ee
    inc bx                                    ; 43                          ; 0xc38f1 vgabios.c:2445
    inc bx                                    ; 43                          ; 0xc38f2
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc38f3 vgabios.c:2447
    je short 03936h                           ; 74 3d                       ; 0xc38f7
    inc bx                                    ; 43                          ; 0xc38f9 vgabios.c:2448
    mov es, cx                                ; 8e c1                       ; 0xc38fa vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc38fc
    xor ah, ah                                ; 30 e4                       ; 0xc38ff vgabios.c:48
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3901
    inc bx                                    ; 43                          ; 0xc3904 vgabios.c:2449
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3905 vgabios.c:47
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3908 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc390b
    inc bx                                    ; 43                          ; 0xc390c vgabios.c:2450
    xor al, al                                ; 30 c0                       ; 0xc390d
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc390f
    out DX, AL                                ; ee                          ; 0xc3912
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3913 vgabios.c:2453
    jmp short 0391fh                          ; eb 07                       ; 0xc3916
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc3918
    jnc short 0392eh                          ; 73 0f                       ; 0xc391d
    mov es, cx                                ; 8e c1                       ; 0xc391f vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3921
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3924 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3927
    inc bx                                    ; 43                          ; 0xc3928 vgabios.c:2454
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3929 vgabios.c:2455
    jmp short 03918h                          ; eb ea                       ; 0xc392c
    inc bx                                    ; 43                          ; 0xc392e vgabios.c:2456
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc392f
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3932
    out DX, AL                                ; ee                          ; 0xc3935
    mov ax, bx                                ; 89 d8                       ; 0xc3936 vgabios.c:2460
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3938
    pop di                                    ; 5f                          ; 0xc393b
    pop si                                    ; 5e                          ; 0xc393c
    pop cx                                    ; 59                          ; 0xc393d
    pop bp                                    ; 5d                          ; 0xc393e
    retn                                      ; c3                          ; 0xc393f
  ; disGetNextSymbol 0xc3940 LB 0xccd -> off=0x0 cb=000000000000002b uValue=00000000000c3940 'find_vga_entry'
find_vga_entry:                              ; 0xc3940 LB 0x2b
    push bx                                   ; 53                          ; 0xc3940 vgabios.c:2469
    push cx                                   ; 51                          ; 0xc3941
    push dx                                   ; 52                          ; 0xc3942
    push bp                                   ; 55                          ; 0xc3943
    mov bp, sp                                ; 89 e5                       ; 0xc3944
    mov dl, al                                ; 88 c2                       ; 0xc3946
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc3948 vgabios.c:2471
    xor al, al                                ; 30 c0                       ; 0xc394a vgabios.c:2472
    jmp short 03954h                          ; eb 06                       ; 0xc394c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc394e vgabios.c:2473
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3950
    jnbe short 03964h                         ; 77 10                       ; 0xc3952
    mov bl, al                                ; 88 c3                       ; 0xc3954
    xor bh, bh                                ; 30 ff                       ; 0xc3956
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3958
    sal bx, CL                                ; d3 e3                       ; 0xc395a
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc395c
    jne short 0394eh                          ; 75 ec                       ; 0xc3960
    mov ah, al                                ; 88 c4                       ; 0xc3962
    mov al, ah                                ; 88 e0                       ; 0xc3964 vgabios.c:2478
    pop bp                                    ; 5d                          ; 0xc3966
    pop dx                                    ; 5a                          ; 0xc3967
    pop cx                                    ; 59                          ; 0xc3968
    pop bx                                    ; 5b                          ; 0xc3969
    retn                                      ; c3                          ; 0xc396a
  ; disGetNextSymbol 0xc396b LB 0xca2 -> off=0x0 cb=000000000000000e uValue=00000000000c396b 'readx_byte'
readx_byte:                                  ; 0xc396b LB 0xe
    push bx                                   ; 53                          ; 0xc396b vgabios.c:2490
    push bp                                   ; 55                          ; 0xc396c
    mov bp, sp                                ; 89 e5                       ; 0xc396d
    mov bx, dx                                ; 89 d3                       ; 0xc396f
    mov es, ax                                ; 8e c0                       ; 0xc3971 vgabios.c:2492
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3973
    pop bp                                    ; 5d                          ; 0xc3976 vgabios.c:2493
    pop bx                                    ; 5b                          ; 0xc3977
    retn                                      ; c3                          ; 0xc3978
  ; disGetNextSymbol 0xc3979 LB 0xc94 -> off=0x8a cb=0000000000000447 uValue=00000000000c3a03 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 043h, 03eh, 02dh, 03ah, 06ah, 03ah, 077h, 03ah, 085h, 03ah
    db  095h, 03ah, 0a5h, 03ah, 0afh, 03ah, 0e1h, 03ah, 006h, 03bh, 014h, 03bh, 02ch, 03bh, 042h, 03bh
    db  06ch, 03bh, 08fh, 03bh, 0a5h, 03bh, 0b1h, 03bh, 08eh, 03ch, 011h, 03dh, 034h, 03dh, 048h, 03dh
    db  08ah, 03dh, 015h, 03eh, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 003h
    db  002h, 001h, 000h, 043h, 03eh, 0d0h, 03bh, 0eeh, 03bh, 0fdh, 03bh, 00ch, 03ch, 016h, 03ch, 0d0h
    db  03bh, 0eeh, 03bh, 0fdh, 03bh, 016h, 03ch, 025h, 03ch, 031h, 03ch, 04ah, 03ch, 059h, 03ch, 068h
    db  03ch, 077h, 03ch, 00ah, 009h, 006h, 004h, 002h, 001h, 000h, 007h, 03eh, 0b0h, 03dh, 0beh, 03dh
    db  0cfh, 03dh, 0dfh, 03dh, 0f4h, 03dh, 007h, 03eh, 007h, 03eh
int10_func:                                  ; 0xc3a03 LB 0x447
    push bp                                   ; 55                          ; 0xc3a03 vgabios.c:2571
    mov bp, sp                                ; 89 e5                       ; 0xc3a04
    push si                                   ; 56                          ; 0xc3a06
    push di                                   ; 57                          ; 0xc3a07
    push ax                                   ; 50                          ; 0xc3a08
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3a09
    mov al, byte [bp+013h]                    ; 8a 46 13                    ; 0xc3a0c vgabios.c:2576
    xor ah, ah                                ; 30 e4                       ; 0xc3a0f
    mov dx, ax                                ; 89 c2                       ; 0xc3a11
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc3a13
    jnbe short 03a82h                         ; 77 6a                       ; 0xc3a16
    push CS                                   ; 0e                          ; 0xc3a18
    pop ES                                    ; 07                          ; 0xc3a19
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3a1a
    mov di, 03979h                            ; bf 79 39                    ; 0xc3a1d
    repne scasb                               ; f2 ae                       ; 0xc3a20
    sal cx, 1                                 ; d1 e1                       ; 0xc3a22
    mov di, cx                                ; 89 cf                       ; 0xc3a24
    mov ax, word [cs:di+0398fh]               ; 2e 8b 85 8f 39              ; 0xc3a26
    jmp ax                                    ; ff e0                       ; 0xc3a2b
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a2d vgabios.c:2579
    xor ah, ah                                ; 30 e4                       ; 0xc3a30
    call 01479h                               ; e8 44 da                    ; 0xc3a32
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a35 vgabios.c:2580
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3a38
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc3a3b
    je short 03a55h                           ; 74 15                       ; 0xc3a3e
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc3a40
    je short 03a4ch                           ; 74 07                       ; 0xc3a43
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc3a45
    jbe short 03a55h                          ; 76 0b                       ; 0xc3a48
    jmp short 03a5eh                          ; eb 12                       ; 0xc3a4a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a4c vgabios.c:2582
    xor al, al                                ; 30 c0                       ; 0xc3a4f
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc3a51
    jmp short 03a65h                          ; eb 10                       ; 0xc3a53 vgabios.c:2583
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a55 vgabios.c:2591
    xor al, al                                ; 30 c0                       ; 0xc3a58
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc3a5a
    jmp short 03a65h                          ; eb 07                       ; 0xc3a5c
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a5e vgabios.c:2594
    xor al, al                                ; 30 c0                       ; 0xc3a61
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc3a63
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3a65
    jmp short 03a82h                          ; eb 18                       ; 0xc3a68 vgabios.c:2596
    mov dl, byte [bp+010h]                    ; 8a 56 10                    ; 0xc3a6a vgabios.c:2598
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc3a6d
    xor ah, ah                                ; 30 e4                       ; 0xc3a70
    call 011d3h                               ; e8 5e d7                    ; 0xc3a72
    jmp short 03a82h                          ; eb 0b                       ; 0xc3a75 vgabios.c:2599
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc3a77 vgabios.c:2601
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3a7a
    xor ah, ah                                ; 30 e4                       ; 0xc3a7d
    call 012cfh                               ; e8 4d d8                    ; 0xc3a7f
    jmp near 03e43h                           ; e9 be 03                    ; 0xc3a82 vgabios.c:2602
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3a85 vgabios.c:2604
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc3a88
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3a8b
    xor ah, ah                                ; 30 e4                       ; 0xc3a8e
    call 00a97h                               ; e8 04 d0                    ; 0xc3a90
    jmp short 03a82h                          ; eb ed                       ; 0xc3a93 vgabios.c:2605
    xor ax, ax                                ; 31 c0                       ; 0xc3a95 vgabios.c:2611
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3a97
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3a9a vgabios.c:2612
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3a9d vgabios.c:2613
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3aa0 vgabios.c:2614
    jmp short 03a82h                          ; eb dd                       ; 0xc3aa3 vgabios.c:2615
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3aa5 vgabios.c:2617
    xor ah, ah                                ; 30 e4                       ; 0xc3aa8
    call 0135ch                               ; e8 af d8                    ; 0xc3aaa
    jmp short 03a82h                          ; eb d3                       ; 0xc3aad vgabios.c:2618
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3aaf vgabios.c:2620
    push ax                                   ; 50                          ; 0xc3ab2
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3ab3
    push ax                                   ; 50                          ; 0xc3ab6
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3ab7
    xor ah, ah                                ; 30 e4                       ; 0xc3aba
    push ax                                   ; 50                          ; 0xc3abc
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc3abd
    push ax                                   ; 50                          ; 0xc3ac0
    mov cl, byte [bp+010h]                    ; 8a 4e 10                    ; 0xc3ac1
    xor ch, ch                                ; 30 ed                       ; 0xc3ac4
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc3ac6
    mov bx, ax                                ; 89 c3                       ; 0xc3ac9
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3acb
    mov dx, ax                                ; 89 c2                       ; 0xc3ace
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3ad0
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3ad3
    mov byte [bp-005h], ah                    ; 88 66 fb                    ; 0xc3ad6
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3ad9
    call 01c71h                               ; e8 92 e1                    ; 0xc3adc
    jmp short 03a82h                          ; eb a1                       ; 0xc3adf vgabios.c:2621
    xor ax, ax                                ; 31 c0                       ; 0xc3ae1 vgabios.c:2623
    push ax                                   ; 50                          ; 0xc3ae3
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3ae4
    push ax                                   ; 50                          ; 0xc3ae7
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3ae8
    xor ah, ah                                ; 30 e4                       ; 0xc3aeb
    push ax                                   ; 50                          ; 0xc3aed
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc3aee
    push ax                                   ; 50                          ; 0xc3af1
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3af2
    mov cx, ax                                ; 89 c1                       ; 0xc3af5
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc3af7
    mov bx, ax                                ; 89 c3                       ; 0xc3afa
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3afc
    mov dx, ax                                ; 89 c2                       ; 0xc3aff
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b01
    jmp short 03adch                          ; eb d6                       ; 0xc3b04
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3b06 vgabios.c:2626
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3b09
    xor ah, ah                                ; 30 e4                       ; 0xc3b0c
    call 00dedh                               ; e8 dc d2                    ; 0xc3b0e
    jmp near 03e43h                           ; e9 2f 03                    ; 0xc3b11 vgabios.c:2627
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3b14 vgabios.c:2629
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3b17
    xor ah, ah                                ; 30 e4                       ; 0xc3b1a
    mov bx, ax                                ; 89 c3                       ; 0xc3b1c
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3b1e
    mov dx, ax                                ; 89 c2                       ; 0xc3b21
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b23
    call 025d9h                               ; e8 b0 ea                    ; 0xc3b26
    jmp near 03e43h                           ; e9 17 03                    ; 0xc3b29 vgabios.c:2630
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3b2c vgabios.c:2632
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3b2f
    xor ah, ah                                ; 30 e4                       ; 0xc3b32
    mov bx, ax                                ; 89 c3                       ; 0xc3b34
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc3b36
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b39
    call 0274bh                               ; e8 0c ec                    ; 0xc3b3c
    jmp near 03e43h                           ; e9 01 03                    ; 0xc3b3f vgabios.c:2633
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc3b42 vgabios.c:2635
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3b45
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc3b48
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3b4b
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3b4e
    mov byte [bp-005h], dh                    ; 88 76 fb                    ; 0xc3b51
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3b54
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3b57
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3b5a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3b5d
    mov byte [bp-005h], dh                    ; 88 76 fb                    ; 0xc3b60
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3b63
    call 028ceh                               ; e8 65 ed                    ; 0xc3b66
    jmp near 03e43h                           ; e9 d7 02                    ; 0xc3b69 vgabios.c:2636
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc3b6c vgabios.c:2638
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3b6f
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3b72
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3b75
    xor ah, ah                                ; 30 e4                       ; 0xc3b78
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3b7a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3b7d
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3b80
    mov byte [bp-005h], ah                    ; 88 66 fb                    ; 0xc3b83
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3b86
    call 00fc7h                               ; e8 3b d4                    ; 0xc3b89
    jmp near 03e43h                           ; e9 b4 02                    ; 0xc3b8c vgabios.c:2639
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3b8f vgabios.c:2647
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3b92
    xor bh, bh                                ; 30 ff                       ; 0xc3b95
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc3b97
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b9a
    xor ah, ah                                ; 30 e4                       ; 0xc3b9d
    call 02a48h                               ; e8 a6 ee                    ; 0xc3b9f
    jmp near 03e43h                           ; e9 9e 02                    ; 0xc3ba2 vgabios.c:2648
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3ba5 vgabios.c:2651
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3ba8
    call 01134h                               ; e8 86 d5                    ; 0xc3bab
    jmp near 03e43h                           ; e9 92 02                    ; 0xc3bae vgabios.c:2652
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3bb1 vgabios.c:2654
    xor ah, ah                                ; 30 e4                       ; 0xc3bb4
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3bb6
    jnbe short 03c2eh                         ; 77 73                       ; 0xc3bb9
    push CS                                   ; 0e                          ; 0xc3bbb
    pop ES                                    ; 07                          ; 0xc3bbc
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3bbd
    mov di, 039bdh                            ; bf bd 39                    ; 0xc3bc0
    repne scasb                               ; f2 ae                       ; 0xc3bc3
    sal cx, 1                                 ; d1 e1                       ; 0xc3bc5
    mov di, cx                                ; 89 cf                       ; 0xc3bc7
    mov ax, word [cs:di+039cch]               ; 2e 8b 85 cc 39              ; 0xc3bc9
    jmp ax                                    ; ff e0                       ; 0xc3bce
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3bd0 vgabios.c:2658
    xor ah, ah                                ; 30 e4                       ; 0xc3bd3
    push ax                                   ; 50                          ; 0xc3bd5
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3bd6
    push ax                                   ; 50                          ; 0xc3bd9
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3bda
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3bdd
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3be0
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3be3
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3be6
    call 02de9h                               ; e8 fd f1                    ; 0xc3be9
    jmp short 03c2eh                          ; eb 40                       ; 0xc3bec vgabios.c:2659
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3bee vgabios.c:2662
    xor dh, dh                                ; 30 f6                       ; 0xc3bf1
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3bf3
    xor ah, ah                                ; 30 e4                       ; 0xc3bf6
    call 02e6dh                               ; e8 72 f2                    ; 0xc3bf8
    jmp short 03c2eh                          ; eb 31                       ; 0xc3bfb vgabios.c:2663
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3bfd vgabios.c:2666
    xor dh, dh                                ; 30 f6                       ; 0xc3c00
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3c02
    xor ah, ah                                ; 30 e4                       ; 0xc3c05
    call 02ee2h                               ; e8 d8 f2                    ; 0xc3c07
    jmp short 03c2eh                          ; eb 22                       ; 0xc3c0a vgabios.c:2667
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c0c vgabios.c:2669
    xor ah, ah                                ; 30 e4                       ; 0xc3c0f
    call 02dc9h                               ; e8 b5 f1                    ; 0xc3c11
    jmp short 03c2eh                          ; eb 18                       ; 0xc3c14 vgabios.c:2670
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3c16 vgabios.c:2673
    xor dh, dh                                ; 30 f6                       ; 0xc3c19
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3c1b
    xor ah, ah                                ; 30 e4                       ; 0xc3c1e
    call 02f55h                               ; e8 32 f3                    ; 0xc3c20
    jmp short 03c2eh                          ; eb 09                       ; 0xc3c23 vgabios.c:2674
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3c25 vgabios.c:2676
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3c28
    call 02fc8h                               ; e8 9a f3                    ; 0xc3c2b
    jmp near 03e43h                           ; e9 12 02                    ; 0xc3c2e vgabios.c:2677
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c31 vgabios.c:2679
    xor ah, ah                                ; 30 e4                       ; 0xc3c34
    push ax                                   ; 50                          ; 0xc3c36
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c37
    mov cx, ax                                ; 89 c1                       ; 0xc3c3a
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3c3c
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3c3f
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3c42
    call 0302bh                               ; e8 e3 f3                    ; 0xc3c45
    jmp short 03c2eh                          ; eb e4                       ; 0xc3c48 vgabios.c:2680
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c4a vgabios.c:2682
    xor ah, ah                                ; 30 e4                       ; 0xc3c4d
    mov dx, ax                                ; 89 c2                       ; 0xc3c4f
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c51
    call 03048h                               ; e8 f1 f3                    ; 0xc3c54
    jmp short 03c2eh                          ; eb d5                       ; 0xc3c57 vgabios.c:2683
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c59 vgabios.c:2685
    xor ah, ah                                ; 30 e4                       ; 0xc3c5c
    mov dx, ax                                ; 89 c2                       ; 0xc3c5e
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c60
    call 0306ah                               ; e8 04 f4                    ; 0xc3c63
    jmp short 03c2eh                          ; eb c6                       ; 0xc3c66 vgabios.c:2686
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c68 vgabios.c:2688
    xor ah, ah                                ; 30 e4                       ; 0xc3c6b
    mov dx, ax                                ; 89 c2                       ; 0xc3c6d
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c6f
    call 0308ch                               ; e8 17 f4                    ; 0xc3c72
    jmp short 03c2eh                          ; eb b7                       ; 0xc3c75 vgabios.c:2689
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc3c77 vgabios.c:2691
    push ax                                   ; 50                          ; 0xc3c7a
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc3c7b
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc3c7e
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3c81
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3c84
    xor ah, ah                                ; 30 e4                       ; 0xc3c87
    call 00f44h                               ; e8 b8 d2                    ; 0xc3c89
    jmp short 03c2eh                          ; eb a0                       ; 0xc3c8c vgabios.c:2699
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3c8e vgabios.c:2701
    xor ah, ah                                ; 30 e4                       ; 0xc3c91
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc3c93
    jc short 03ca6h                           ; 72 0e                       ; 0xc3c96
    jbe short 03cd1h                          ; 76 37                       ; 0xc3c98
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc3c9a
    je short 03cf9h                           ; 74 5a                       ; 0xc3c9d
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3c9f
    je short 03cfbh                           ; 74 57                       ; 0xc3ca2
    jmp short 03c2eh                          ; eb 88                       ; 0xc3ca4
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3ca6
    je short 03cb5h                           ; 74 0a                       ; 0xc3ca9
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc3cab
    jne short 03cf6h                          ; 75 46                       ; 0xc3cae
    call 030aeh                               ; e8 fb f3                    ; 0xc3cb0 vgabios.c:2704
    jmp short 03cf6h                          ; eb 41                       ; 0xc3cb3 vgabios.c:2705
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3cb5 vgabios.c:2707
    xor ah, ah                                ; 30 e4                       ; 0xc3cb8
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3cba
    jnbe short 03cf6h                         ; 77 37                       ; 0xc3cbd
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3cbf vgabios.c:2708
    call 030b3h                               ; e8 ee f3                    ; 0xc3cc2
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3cc5 vgabios.c:2709
    xor al, al                                ; 30 c0                       ; 0xc3cc8
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3cca
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3ccc
    jmp short 03cf6h                          ; eb 25                       ; 0xc3ccf vgabios.c:2711
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3cd1 vgabios.c:2713
    xor ah, ah                                ; 30 e4                       ; 0xc3cd4
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3cd6
    jnc short 03cf3h                          ; 73 18                       ; 0xc3cd9
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3cdb vgabios.c:45
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3cde
    mov es, ax                                ; 8e c0                       ; 0xc3ce1 vgabios.c:47
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc3ce3
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc3ce6 vgabios.c:48
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3ce9
    or dl, al                                 ; 08 c2                       ; 0xc3cec
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc3cee vgabios.c:52
    jmp short 03cc5h                          ; eb d2                       ; 0xc3cf1
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc3cf3 vgabios.c:2719
    jmp near 03e43h                           ; e9 4a 01                    ; 0xc3cf6 vgabios.c:2720
    jmp short 03d09h                          ; eb 0e                       ; 0xc3cf9
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3cfb vgabios.c:2722
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3cfe
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d01
    call 030e5h                               ; e8 de f3                    ; 0xc3d04
    jmp short 03cc5h                          ; eb bc                       ; 0xc3d07
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3d09 vgabios.c:2726
    call 030eah                               ; e8 db f3                    ; 0xc3d0c
    jmp short 03cc5h                          ; eb b4                       ; 0xc3d0f
    push word [bp+008h]                       ; ff 76 08                    ; 0xc3d11 vgabios.c:2736
    push word [bp+016h]                       ; ff 76 16                    ; 0xc3d14
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3d17
    xor ah, ah                                ; 30 e4                       ; 0xc3d1a
    push ax                                   ; 50                          ; 0xc3d1c
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc3d1d
    push ax                                   ; 50                          ; 0xc3d20
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3d21
    xor bh, bh                                ; 30 ff                       ; 0xc3d24
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc3d26
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3d29
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3d2c
    call 030efh                               ; e8 bd f3                    ; 0xc3d2f
    jmp short 03cf6h                          ; eb c2                       ; 0xc3d32 vgabios.c:2737
    mov bx, si                                ; 89 f3                       ; 0xc3d34 vgabios.c:2739
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d36
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3d39
    call 0317eh                               ; e8 3f f4                    ; 0xc3d3c
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d3f vgabios.c:2740
    xor al, al                                ; 30 c0                       ; 0xc3d42
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3d44
    jmp short 03ccch                          ; eb 84                       ; 0xc3d46
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d48 vgabios.c:2743
    xor ah, ah                                ; 30 e4                       ; 0xc3d4b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3d4d
    je short 03d74h                           ; 74 22                       ; 0xc3d50
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3d52
    je short 03d66h                           ; 74 0f                       ; 0xc3d55
    test ax, ax                               ; 85 c0                       ; 0xc3d57
    jne short 03d80h                          ; 75 25                       ; 0xc3d59
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3d5b vgabios.c:2746
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d5e
    call 03393h                               ; e8 2f f6                    ; 0xc3d61
    jmp short 03d80h                          ; eb 1a                       ; 0xc3d64 vgabios.c:2747
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3d66 vgabios.c:2749
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d69
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d6c
    call 033aeh                               ; e8 3c f6                    ; 0xc3d6f
    jmp short 03d80h                          ; eb 0c                       ; 0xc3d72 vgabios.c:2750
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3d74 vgabios.c:2752
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d77
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d7a
    call 03686h                               ; e8 06 f9                    ; 0xc3d7d
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d80 vgabios.c:2759
    xor al, al                                ; 30 c0                       ; 0xc3d83
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3d85
    jmp near 03ccch                           ; e9 42 ff                    ; 0xc3d87
    call 007f8h                               ; e8 6b ca                    ; 0xc3d8a vgabios.c:2764
    test ax, ax                               ; 85 c0                       ; 0xc3d8d
    je short 03e05h                           ; 74 74                       ; 0xc3d8f
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d91 vgabios.c:2765
    xor ah, ah                                ; 30 e4                       ; 0xc3d94
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3d96
    jnbe short 03e07h                         ; 77 6c                       ; 0xc3d99
    push CS                                   ; 0e                          ; 0xc3d9b
    pop ES                                    ; 07                          ; 0xc3d9c
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3d9d
    mov di, 039ech                            ; bf ec 39                    ; 0xc3da0
    repne scasb                               ; f2 ae                       ; 0xc3da3
    sal cx, 1                                 ; d1 e1                       ; 0xc3da5
    mov di, cx                                ; 89 cf                       ; 0xc3da7
    mov ax, word [cs:di+039f3h]               ; 2e 8b 85 f3 39              ; 0xc3da9
    jmp ax                                    ; ff e0                       ; 0xc3dae
    mov bx, si                                ; 89 f3                       ; 0xc3db0 vgabios.c:2768
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3db2
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3db5
    call 04014h                               ; e8 59 02                    ; 0xc3db8
    jmp near 03e43h                           ; e9 85 00                    ; 0xc3dbb vgabios.c:2769
    mov cx, si                                ; 89 f1                       ; 0xc3dbe vgabios.c:2771
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3dc0
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3dc3
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dc6
    call 0413fh                               ; e8 73 03                    ; 0xc3dc9
    jmp near 03e43h                           ; e9 74 00                    ; 0xc3dcc vgabios.c:2772
    mov cx, si                                ; 89 f1                       ; 0xc3dcf vgabios.c:2774
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3dd1
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3dd4
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dd7
    call 041deh                               ; e8 01 04                    ; 0xc3dda
    jmp short 03e43h                          ; eb 64                       ; 0xc3ddd vgabios.c:2775
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3ddf vgabios.c:2777
    push ax                                   ; 50                          ; 0xc3de2
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3de3
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3de6
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3de9
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dec
    call 043a7h                               ; e8 b5 05                    ; 0xc3def
    jmp short 03e43h                          ; eb 4f                       ; 0xc3df2 vgabios.c:2778
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3df4 vgabios.c:2780
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3df7
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3dfa
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dfd
    call 04434h                               ; e8 31 06                    ; 0xc3e00
    jmp short 03e43h                          ; eb 3e                       ; 0xc3e03 vgabios.c:2781
    jmp short 03e0eh                          ; eb 07                       ; 0xc3e05
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3e07 vgabios.c:2803
    jmp short 03e43h                          ; eb 35                       ; 0xc3e0c vgabios.c:2806
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3e0e vgabios.c:2808
    jmp short 03e43h                          ; eb 2e                       ; 0xc3e13 vgabios.c:2810
    call 007f8h                               ; e8 e0 c9                    ; 0xc3e15 vgabios.c:2812
    test ax, ax                               ; 85 c0                       ; 0xc3e18
    je short 03e3eh                           ; 74 22                       ; 0xc3e1a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3e1c vgabios.c:2813
    xor ah, ah                                ; 30 e4                       ; 0xc3e1f
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3e21
    jne short 03e37h                          ; 75 11                       ; 0xc3e24
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3e26 vgabios.c:2816
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3e29
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3e2c
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3e2f
    call 04516h                               ; e8 e1 06                    ; 0xc3e32
    jmp short 03e43h                          ; eb 0c                       ; 0xc3e35 vgabios.c:2817
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3e37 vgabios.c:2819
    jmp short 03e43h                          ; eb 05                       ; 0xc3e3c vgabios.c:2822
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3e3e vgabios.c:2824
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e43 vgabios.c:2834
    pop di                                    ; 5f                          ; 0xc3e46
    pop si                                    ; 5e                          ; 0xc3e47
    pop bp                                    ; 5d                          ; 0xc3e48
    retn                                      ; c3                          ; 0xc3e49
  ; disGetNextSymbol 0xc3e4a LB 0x7c3 -> off=0x0 cb=000000000000001f uValue=00000000000c3e4a 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3e4a LB 0x1f
    push bp                                   ; 55                          ; 0xc3e4a vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3e4b
    push bx                                   ; 53                          ; 0xc3e4d
    push dx                                   ; 52                          ; 0xc3e4e
    mov bx, ax                                ; 89 c3                       ; 0xc3e4f
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e51 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e54
    call 005a0h                               ; e8 46 c7                    ; 0xc3e57
    mov ax, bx                                ; 89 d8                       ; 0xc3e5a vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e5c
    call 005a0h                               ; e8 3e c7                    ; 0xc3e5f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e62 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3e65
    pop bx                                    ; 5b                          ; 0xc3e66
    pop bp                                    ; 5d                          ; 0xc3e67
    retn                                      ; c3                          ; 0xc3e68
  ; disGetNextSymbol 0xc3e69 LB 0x7a4 -> off=0x0 cb=000000000000001f uValue=00000000000c3e69 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3e69 LB 0x1f
    push bp                                   ; 55                          ; 0xc3e69 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3e6a
    push bx                                   ; 53                          ; 0xc3e6c
    push dx                                   ; 52                          ; 0xc3e6d
    mov bx, ax                                ; 89 c3                       ; 0xc3e6e
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3e70 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e73
    call 005a0h                               ; e8 27 c7                    ; 0xc3e76
    mov ax, bx                                ; 89 d8                       ; 0xc3e79 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e7b
    call 005a0h                               ; e8 1f c7                    ; 0xc3e7e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e81 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3e84
    pop bx                                    ; 5b                          ; 0xc3e85
    pop bp                                    ; 5d                          ; 0xc3e86
    retn                                      ; c3                          ; 0xc3e87
  ; disGetNextSymbol 0xc3e88 LB 0x785 -> off=0x0 cb=0000000000000019 uValue=00000000000c3e88 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3e88 LB 0x19
    push bp                                   ; 55                          ; 0xc3e88 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3e89
    push dx                                   ; 52                          ; 0xc3e8b
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3e8c vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e8f
    call 005a0h                               ; e8 0b c7                    ; 0xc3e92
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e95 vbe.c:121
    call 005a7h                               ; e8 0c c7                    ; 0xc3e98
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3e9b vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3e9e
    pop bp                                    ; 5d                          ; 0xc3e9f
    retn                                      ; c3                          ; 0xc3ea0
  ; disGetNextSymbol 0xc3ea1 LB 0x76c -> off=0x0 cb=000000000000001f uValue=00000000000c3ea1 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3ea1 LB 0x1f
    push bp                                   ; 55                          ; 0xc3ea1 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3ea2
    push bx                                   ; 53                          ; 0xc3ea4
    push dx                                   ; 52                          ; 0xc3ea5
    mov bx, ax                                ; 89 c3                       ; 0xc3ea6
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3ea8 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3eab
    call 005a0h                               ; e8 ef c6                    ; 0xc3eae
    mov ax, bx                                ; 89 d8                       ; 0xc3eb1 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3eb3
    call 005a0h                               ; e8 e7 c6                    ; 0xc3eb6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3eb9 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3ebc
    pop bx                                    ; 5b                          ; 0xc3ebd
    pop bp                                    ; 5d                          ; 0xc3ebe
    retn                                      ; c3                          ; 0xc3ebf
  ; disGetNextSymbol 0xc3ec0 LB 0x74d -> off=0x0 cb=0000000000000019 uValue=00000000000c3ec0 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3ec0 LB 0x19
    push bp                                   ; 55                          ; 0xc3ec0 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3ec1
    push dx                                   ; 52                          ; 0xc3ec3
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3ec4 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ec7
    call 005a0h                               ; e8 d3 c6                    ; 0xc3eca
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ecd vbe.c:136
    call 005a7h                               ; e8 d4 c6                    ; 0xc3ed0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ed3 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3ed6
    pop bp                                    ; 5d                          ; 0xc3ed7
    retn                                      ; c3                          ; 0xc3ed8
  ; disGetNextSymbol 0xc3ed9 LB 0x734 -> off=0x0 cb=000000000000001f uValue=00000000000c3ed9 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3ed9 LB 0x1f
    push bp                                   ; 55                          ; 0xc3ed9 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3eda
    push bx                                   ; 53                          ; 0xc3edc
    push dx                                   ; 52                          ; 0xc3edd
    mov bx, ax                                ; 89 c3                       ; 0xc3ede
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3ee0 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ee3
    call 005a0h                               ; e8 b7 c6                    ; 0xc3ee6
    mov ax, bx                                ; 89 d8                       ; 0xc3ee9 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3eeb
    call 005a0h                               ; e8 af c6                    ; 0xc3eee
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ef1 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3ef4
    pop bx                                    ; 5b                          ; 0xc3ef5
    pop bp                                    ; 5d                          ; 0xc3ef6
    retn                                      ; c3                          ; 0xc3ef7
  ; disGetNextSymbol 0xc3ef8 LB 0x715 -> off=0x0 cb=0000000000000019 uValue=00000000000c3ef8 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3ef8 LB 0x19
    push bp                                   ; 55                          ; 0xc3ef8 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3ef9
    push dx                                   ; 52                          ; 0xc3efb
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3efc vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3eff
    call 005a0h                               ; e8 9b c6                    ; 0xc3f02
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f05 vbe.c:151
    call 005a7h                               ; e8 9c c6                    ; 0xc3f08
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f0b vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3f0e
    pop bp                                    ; 5d                          ; 0xc3f0f
    retn                                      ; c3                          ; 0xc3f10
  ; disGetNextSymbol 0xc3f11 LB 0x6fc -> off=0x0 cb=0000000000000019 uValue=00000000000c3f11 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3f11 LB 0x19
    push bp                                   ; 55                          ; 0xc3f11 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3f12
    push dx                                   ; 52                          ; 0xc3f14
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3f15 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f18
    call 005a0h                               ; e8 82 c6                    ; 0xc3f1b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f1e vbe.c:157
    call 005a7h                               ; e8 83 c6                    ; 0xc3f21
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f24 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3f27
    pop bp                                    ; 5d                          ; 0xc3f28
    retn                                      ; c3                          ; 0xc3f29
  ; disGetNextSymbol 0xc3f2a LB 0x6e3 -> off=0x0 cb=0000000000000012 uValue=00000000000c3f2a 'in_word'
in_word:                                     ; 0xc3f2a LB 0x12
    push bp                                   ; 55                          ; 0xc3f2a vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3f2b
    push bx                                   ; 53                          ; 0xc3f2d
    mov bx, ax                                ; 89 c3                       ; 0xc3f2e
    mov ax, dx                                ; 89 d0                       ; 0xc3f30
    mov dx, bx                                ; 89 da                       ; 0xc3f32 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3f34
    in ax, DX                                 ; ed                          ; 0xc3f35 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f36 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3f39
    pop bp                                    ; 5d                          ; 0xc3f3a
    retn                                      ; c3                          ; 0xc3f3b
  ; disGetNextSymbol 0xc3f3c LB 0x6d1 -> off=0x0 cb=0000000000000014 uValue=00000000000c3f3c 'in_byte'
in_byte:                                     ; 0xc3f3c LB 0x14
    push bp                                   ; 55                          ; 0xc3f3c vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3f3d
    push bx                                   ; 53                          ; 0xc3f3f
    mov bx, ax                                ; 89 c3                       ; 0xc3f40
    mov ax, dx                                ; 89 d0                       ; 0xc3f42
    mov dx, bx                                ; 89 da                       ; 0xc3f44 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3f46
    in AL, DX                                 ; ec                          ; 0xc3f47 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3f48
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f4a vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3f4d
    pop bp                                    ; 5d                          ; 0xc3f4e
    retn                                      ; c3                          ; 0xc3f4f
  ; disGetNextSymbol 0xc3f50 LB 0x6bd -> off=0x0 cb=0000000000000014 uValue=00000000000c3f50 'dispi_get_id'
dispi_get_id:                                ; 0xc3f50 LB 0x14
    push bp                                   ; 55                          ; 0xc3f50 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3f51
    push dx                                   ; 52                          ; 0xc3f53
    xor ax, ax                                ; 31 c0                       ; 0xc3f54 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f56
    out DX, ax                                ; ef                          ; 0xc3f59
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f5a vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3f5d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f5e vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3f61
    pop bp                                    ; 5d                          ; 0xc3f62
    retn                                      ; c3                          ; 0xc3f63
  ; disGetNextSymbol 0xc3f64 LB 0x6a9 -> off=0x0 cb=000000000000001a uValue=00000000000c3f64 'dispi_set_id'
dispi_set_id:                                ; 0xc3f64 LB 0x1a
    push bp                                   ; 55                          ; 0xc3f64 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3f65
    push bx                                   ; 53                          ; 0xc3f67
    push dx                                   ; 52                          ; 0xc3f68
    mov bx, ax                                ; 89 c3                       ; 0xc3f69
    xor ax, ax                                ; 31 c0                       ; 0xc3f6b vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f6d
    out DX, ax                                ; ef                          ; 0xc3f70
    mov ax, bx                                ; 89 d8                       ; 0xc3f71 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f73
    out DX, ax                                ; ef                          ; 0xc3f76
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f77 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3f7a
    pop bx                                    ; 5b                          ; 0xc3f7b
    pop bp                                    ; 5d                          ; 0xc3f7c
    retn                                      ; c3                          ; 0xc3f7d
  ; disGetNextSymbol 0xc3f7e LB 0x68f -> off=0x0 cb=000000000000002a uValue=00000000000c3f7e 'vbe_init'
vbe_init:                                    ; 0xc3f7e LB 0x2a
    push bp                                   ; 55                          ; 0xc3f7e vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3f7f
    push bx                                   ; 53                          ; 0xc3f81
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3f82 vbe.c:190
    call 03f64h                               ; e8 dc ff                    ; 0xc3f85
    call 03f50h                               ; e8 c5 ff                    ; 0xc3f88 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3f8b
    jne short 03fa2h                          ; 75 12                       ; 0xc3f8e
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3f90 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f93
    mov es, ax                                ; 8e c0                       ; 0xc3f96
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3f98
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3f9c vbe.c:194
    call 03f64h                               ; e8 c2 ff                    ; 0xc3f9f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3fa2 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3fa5
    pop bp                                    ; 5d                          ; 0xc3fa6
    retn                                      ; c3                          ; 0xc3fa7
  ; disGetNextSymbol 0xc3fa8 LB 0x665 -> off=0x0 cb=000000000000006c uValue=00000000000c3fa8 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3fa8 LB 0x6c
    push bp                                   ; 55                          ; 0xc3fa8 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3fa9
    push bx                                   ; 53                          ; 0xc3fab
    push cx                                   ; 51                          ; 0xc3fac
    push si                                   ; 56                          ; 0xc3fad
    push di                                   ; 57                          ; 0xc3fae
    mov di, ax                                ; 89 c7                       ; 0xc3faf
    mov si, dx                                ; 89 d6                       ; 0xc3fb1
    xor dx, dx                                ; 31 d2                       ; 0xc3fb3 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fb5
    call 03f2ah                               ; e8 6f ff                    ; 0xc3fb8
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3fbb vbe.c:209
    jne short 04009h                          ; 75 49                       ; 0xc3fbe
    test si, si                               ; 85 f6                       ; 0xc3fc0 vbe.c:213
    je short 03fd7h                           ; 74 13                       ; 0xc3fc2
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3fc4 vbe.c:220
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fc7
    call 005a0h                               ; e8 d3 c5                    ; 0xc3fca
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fcd vbe.c:221
    call 005a7h                               ; e8 d4 c5                    ; 0xc3fd0
    test ax, ax                               ; 85 c0                       ; 0xc3fd3 vbe.c:222
    je short 0400bh                           ; 74 34                       ; 0xc3fd5
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3fd7 vbe.c:226
    mov dx, bx                                ; 89 da                       ; 0xc3fda vbe.c:232
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fdc
    call 03f2ah                               ; e8 48 ff                    ; 0xc3fdf
    mov cx, ax                                ; 89 c1                       ; 0xc3fe2
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3fe4 vbe.c:233
    je short 04009h                           ; 74 20                       ; 0xc3fe7
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3fe9 vbe.c:235
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fec
    call 03f2ah                               ; e8 38 ff                    ; 0xc3fef
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3ff2
    cmp cx, di                                ; 39 f9                       ; 0xc3ff5 vbe.c:237
    jne short 04005h                          ; 75 0c                       ; 0xc3ff7
    test si, si                               ; 85 f6                       ; 0xc3ff9 vbe.c:239
    jne short 04001h                          ; 75 04                       ; 0xc3ffb
    mov ax, bx                                ; 89 d8                       ; 0xc3ffd vbe.c:240
    jmp short 0400bh                          ; eb 0a                       ; 0xc3fff
    test AL, strict byte 080h                 ; a8 80                       ; 0xc4001 vbe.c:241
    jne short 03ffdh                          ; 75 f8                       ; 0xc4003
    mov bx, dx                                ; 89 d3                       ; 0xc4005 vbe.c:244
    jmp short 03fdch                          ; eb d3                       ; 0xc4007 vbe.c:249
    xor ax, ax                                ; 31 c0                       ; 0xc4009 vbe.c:252
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc400b vbe.c:253
    pop di                                    ; 5f                          ; 0xc400e
    pop si                                    ; 5e                          ; 0xc400f
    pop cx                                    ; 59                          ; 0xc4010
    pop bx                                    ; 5b                          ; 0xc4011
    pop bp                                    ; 5d                          ; 0xc4012
    retn                                      ; c3                          ; 0xc4013
  ; disGetNextSymbol 0xc4014 LB 0x5f9 -> off=0x0 cb=000000000000012b uValue=00000000000c4014 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc4014 LB 0x12b
    push bp                                   ; 55                          ; 0xc4014 vbe.c:284
    mov bp, sp                                ; 89 e5                       ; 0xc4015
    push cx                                   ; 51                          ; 0xc4017
    push si                                   ; 56                          ; 0xc4018
    push di                                   ; 57                          ; 0xc4019
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc401a
    mov si, ax                                ; 89 c6                       ; 0xc401d
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc401f
    mov di, bx                                ; 89 df                       ; 0xc4022
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc4024 vbe.c:289
    call 005eah                               ; e8 be c5                    ; 0xc4029 vbe.c:292
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc402c
    mov bx, di                                ; 89 fb                       ; 0xc402f vbe.c:295
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4031
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc4034
    xor dx, dx                                ; 31 d2                       ; 0xc4037 vbe.c:298
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4039
    call 03f2ah                               ; e8 eb fe                    ; 0xc403c
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc403f vbe.c:299
    je short 0404eh                           ; 74 0a                       ; 0xc4042
    push SS                                   ; 16                          ; 0xc4044 vbe.c:301
    pop ES                                    ; 07                          ; 0xc4045
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc4046
    jmp near 04137h                           ; e9 e9 00                    ; 0xc404b vbe.c:305
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc404e vbe.c:307
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc4051 vbe.c:314
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc4056 vbe.c:322
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc4059
    jne short 04068h                          ; 75 07                       ; 0xc405f
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc4061
    je short 04077h                           ; 74 0f                       ; 0xc4066
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc4068
    jne short 0407ch                          ; 75 0c                       ; 0xc406e
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc4070
    jne short 0407ch                          ; 75 05                       ; 0xc4075
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc4077 vbe.c:324
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc407c vbe.c:332
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc407f
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc4084 vbe.c:334
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc408a vbe.c:338
    mov word [es:bx+006h], 07e02h             ; 26 c7 47 06 02 7e           ; 0xc4090 vbe.c:341
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc4096
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc409a vbe.c:344
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc40a0 vbe.c:346
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc40a6 vbe.c:350
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc40a9
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc40ad vbe.c:351
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc40b0
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc40b4 vbe.c:354
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc40b7
    call 03f2ah                               ; e8 6d fe                    ; 0xc40ba
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc40bd
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc40c0
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc40c4 vbe.c:356
    je short 040eeh                           ; 74 24                       ; 0xc40c8
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc40ca vbe.c:359
    mov word [es:bx+016h], 07e17h             ; 26 c7 47 16 17 7e           ; 0xc40d0 vbe.c:360
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc40d6
    mov word [es:bx+01ah], 07e34h             ; 26 c7 47 1a 34 7e           ; 0xc40da vbe.c:361
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc40e0
    mov word [es:bx+01eh], 07e55h             ; 26 c7 47 1e 55 7e           ; 0xc40e4 vbe.c:362
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc40ea
    mov dx, cx                                ; 89 ca                       ; 0xc40ee vbe.c:369
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc40f0
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc40f3
    call 03f3ch                               ; e8 43 fe                    ; 0xc40f6
    xor ah, ah                                ; 30 e4                       ; 0xc40f9 vbe.c:370
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc40fb
    jnbe short 04117h                         ; 77 17                       ; 0xc40fe
    mov dx, cx                                ; 89 ca                       ; 0xc4100 vbe.c:372
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4102
    call 03f2ah                               ; e8 22 fe                    ; 0xc4105
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc4108 vbe.c:376
    add bx, di                                ; 01 fb                       ; 0xc410b
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc410d vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4110
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc4113 vbe.c:378
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc4117 vbe.c:380
    mov dx, cx                                ; 89 ca                       ; 0xc411a vbe.c:381
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc411c
    call 03f2ah                               ; e8 08 fe                    ; 0xc411f
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc4122 vbe.c:382
    jne short 040eeh                          ; 75 c7                       ; 0xc4125
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc4127 vbe.c:385
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc412a vbe.c:62
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc412d
    push SS                                   ; 16                          ; 0xc4130 vbe.c:386
    pop ES                                    ; 07                          ; 0xc4131
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc4132
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4137 vbe.c:387
    pop di                                    ; 5f                          ; 0xc413a
    pop si                                    ; 5e                          ; 0xc413b
    pop cx                                    ; 59                          ; 0xc413c
    pop bp                                    ; 5d                          ; 0xc413d
    retn                                      ; c3                          ; 0xc413e
  ; disGetNextSymbol 0xc413f LB 0x4ce -> off=0x0 cb=000000000000009f uValue=00000000000c413f 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc413f LB 0x9f
    push bp                                   ; 55                          ; 0xc413f vbe.c:399
    mov bp, sp                                ; 89 e5                       ; 0xc4140
    push si                                   ; 56                          ; 0xc4142
    push di                                   ; 57                          ; 0xc4143
    push ax                                   ; 50                          ; 0xc4144
    push ax                                   ; 50                          ; 0xc4145
    mov ax, dx                                ; 89 d0                       ; 0xc4146
    mov si, bx                                ; 89 de                       ; 0xc4148
    mov bx, cx                                ; 89 cb                       ; 0xc414a
    test dh, 040h                             ; f6 c6 40                    ; 0xc414c vbe.c:410
    je short 04156h                           ; 74 05                       ; 0xc414f
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc4151
    jmp short 04158h                          ; eb 02                       ; 0xc4154
    xor dx, dx                                ; 31 d2                       ; 0xc4156
    and ah, 001h                              ; 80 e4 01                    ; 0xc4158 vbe.c:411
    call 03fa8h                               ; e8 4a fe                    ; 0xc415b vbe.c:413
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc415e
    test ax, ax                               ; 85 c0                       ; 0xc4161 vbe.c:415
    je short 041cch                           ; 74 67                       ; 0xc4163
    mov cx, 00100h                            ; b9 00 01                    ; 0xc4165 vbe.c:420
    xor ax, ax                                ; 31 c0                       ; 0xc4168
    mov di, bx                                ; 89 df                       ; 0xc416a
    mov es, si                                ; 8e c6                       ; 0xc416c
    jcxz 04172h                               ; e3 02                       ; 0xc416e
    rep stosb                                 ; f3 aa                       ; 0xc4170
    xor cx, cx                                ; 31 c9                       ; 0xc4172 vbe.c:421
    jmp short 0417bh                          ; eb 05                       ; 0xc4174
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc4176
    jnc short 04194h                          ; 73 19                       ; 0xc4179
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc417b vbe.c:424
    inc dx                                    ; 42                          ; 0xc417e
    inc dx                                    ; 42                          ; 0xc417f
    add dx, cx                                ; 01 ca                       ; 0xc4180
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4182
    call 03f3ch                               ; e8 b4 fd                    ; 0xc4185
    mov di, bx                                ; 89 df                       ; 0xc4188 vbe.c:425
    add di, cx                                ; 01 cf                       ; 0xc418a
    mov es, si                                ; 8e c6                       ; 0xc418c vbe.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc418e
    inc cx                                    ; 41                          ; 0xc4191 vbe.c:426
    jmp short 04176h                          ; eb e2                       ; 0xc4192
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc4194 vbe.c:427
    mov es, si                                ; 8e c6                       ; 0xc4197 vbe.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc4199
    test AL, strict byte 001h                 ; a8 01                       ; 0xc419c vbe.c:428
    je short 041b0h                           ; 74 10                       ; 0xc419e
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc41a0 vbe.c:429
    mov word [es:di], 0065ch                  ; 26 c7 05 5c 06              ; 0xc41a3 vbe.c:62
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc41a8 vbe.c:431
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc41ab vbe.c:62
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc41b0 vbe.c:434
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc41b3
    call 005a0h                               ; e8 e7 c3                    ; 0xc41b6
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc41b9 vbe.c:435
    call 005a7h                               ; e8 e8 c3                    ; 0xc41bc
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc41bf
    mov es, si                                ; 8e c6                       ; 0xc41c2 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc41c4
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc41c7 vbe.c:437
    jmp short 041cfh                          ; eb 03                       ; 0xc41ca vbe.c:438
    mov ax, 00100h                            ; b8 00 01                    ; 0xc41cc vbe.c:442
    push SS                                   ; 16                          ; 0xc41cf vbe.c:445
    pop ES                                    ; 07                          ; 0xc41d0
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc41d1
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc41d4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc41d7 vbe.c:446
    pop di                                    ; 5f                          ; 0xc41da
    pop si                                    ; 5e                          ; 0xc41db
    pop bp                                    ; 5d                          ; 0xc41dc
    retn                                      ; c3                          ; 0xc41dd
  ; disGetNextSymbol 0xc41de LB 0x42f -> off=0x0 cb=00000000000000e7 uValue=00000000000c41de 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc41de LB 0xe7
    push bp                                   ; 55                          ; 0xc41de vbe.c:458
    mov bp, sp                                ; 89 e5                       ; 0xc41df
    push si                                   ; 56                          ; 0xc41e1
    push di                                   ; 57                          ; 0xc41e2
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc41e3
    mov si, ax                                ; 89 c6                       ; 0xc41e6
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc41e8
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc41eb vbe.c:466
    je short 041f6h                           ; 74 05                       ; 0xc41ef
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc41f1
    jmp short 041f8h                          ; eb 02                       ; 0xc41f4
    xor ax, ax                                ; 31 c0                       ; 0xc41f6
    mov dx, ax                                ; 89 c2                       ; 0xc41f8
    test ax, ax                               ; 85 c0                       ; 0xc41fa vbe.c:467
    je short 04201h                           ; 74 03                       ; 0xc41fc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41fe
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc4201
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc4204 vbe.c:468
    je short 0420fh                           ; 74 05                       ; 0xc4208
    mov ax, 00080h                            ; b8 80 00                    ; 0xc420a
    jmp short 04211h                          ; eb 02                       ; 0xc420d
    xor ax, ax                                ; 31 c0                       ; 0xc420f
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc4211
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc4214 vbe.c:470
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc4218 vbe.c:473
    jnc short 04232h                          ; 73 13                       ; 0xc421d
    xor ax, ax                                ; 31 c0                       ; 0xc421f vbe.c:477
    call 00610h                               ; e8 ec c3                    ; 0xc4221
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc4224 vbe.c:481
    xor ah, ah                                ; 30 e4                       ; 0xc4227
    call 01479h                               ; e8 4d d2                    ; 0xc4229
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc422c vbe.c:482
    jmp near 042b9h                           ; e9 87 00                    ; 0xc422f vbe.c:483
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4232 vbe.c:486
    call 03fa8h                               ; e8 70 fd                    ; 0xc4235
    mov bx, ax                                ; 89 c3                       ; 0xc4238
    test ax, ax                               ; 85 c0                       ; 0xc423a vbe.c:488
    je short 042b6h                           ; 74 78                       ; 0xc423c
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc423e vbe.c:493
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4241
    call 03f2ah                               ; e8 e3 fc                    ; 0xc4244
    mov cx, ax                                ; 89 c1                       ; 0xc4247
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc4249 vbe.c:494
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc424c
    call 03f2ah                               ; e8 d8 fc                    ; 0xc424f
    mov di, ax                                ; 89 c7                       ; 0xc4252
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc4254 vbe.c:495
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4257
    call 03f3ch                               ; e8 df fc                    ; 0xc425a
    mov bl, al                                ; 88 c3                       ; 0xc425d
    mov dl, al                                ; 88 c2                       ; 0xc425f
    xor ax, ax                                ; 31 c0                       ; 0xc4261 vbe.c:503
    call 00610h                               ; e8 aa c3                    ; 0xc4263
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc4266 vbe.c:505
    jne short 04271h                          ; 75 06                       ; 0xc4269
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc426b vbe.c:507
    call 01479h                               ; e8 08 d2                    ; 0xc426e
    mov al, dl                                ; 88 d0                       ; 0xc4271 vbe.c:510
    xor ah, ah                                ; 30 e4                       ; 0xc4273
    call 03ea1h                               ; e8 29 fc                    ; 0xc4275
    mov ax, cx                                ; 89 c8                       ; 0xc4278 vbe.c:511
    call 03e4ah                               ; e8 cd fb                    ; 0xc427a
    mov ax, di                                ; 89 f8                       ; 0xc427d vbe.c:512
    call 03e69h                               ; e8 e7 fb                    ; 0xc427f
    xor ax, ax                                ; 31 c0                       ; 0xc4282 vbe.c:513
    call 00636h                               ; e8 af c3                    ; 0xc4284
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc4287 vbe.c:514
    or dl, 001h                               ; 80 ca 01                    ; 0xc428a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc428d
    xor ah, ah                                ; 30 e4                       ; 0xc4290
    or al, dl                                 ; 08 d0                       ; 0xc4292
    call 00610h                               ; e8 79 c3                    ; 0xc4294
    call 00708h                               ; e8 6e c4                    ; 0xc4297 vbe.c:515
    mov bx, 000bah                            ; bb ba 00                    ; 0xc429a vbe.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc429d
    mov es, ax                                ; 8e c0                       ; 0xc42a0
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc42a2
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc42a5
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc42a8 vbe.c:518
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc42ab
    mov bx, 00087h                            ; bb 87 00                    ; 0xc42ad vbe.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc42b0
    jmp near 0422ch                           ; e9 76 ff                    ; 0xc42b3
    mov ax, 00100h                            ; b8 00 01                    ; 0xc42b6 vbe.c:527
    push SS                                   ; 16                          ; 0xc42b9 vbe.c:531
    pop ES                                    ; 07                          ; 0xc42ba
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc42bb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc42be vbe.c:532
    pop di                                    ; 5f                          ; 0xc42c1
    pop si                                    ; 5e                          ; 0xc42c2
    pop bp                                    ; 5d                          ; 0xc42c3
    retn                                      ; c3                          ; 0xc42c4
  ; disGetNextSymbol 0xc42c5 LB 0x348 -> off=0x0 cb=0000000000000008 uValue=00000000000c42c5 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc42c5 LB 0x8
    push bp                                   ; 55                          ; 0xc42c5 vbe.c:534
    mov bp, sp                                ; 89 e5                       ; 0xc42c6
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc42c8 vbe.c:537
    pop bp                                    ; 5d                          ; 0xc42cb
    retn                                      ; c3                          ; 0xc42cc
  ; disGetNextSymbol 0xc42cd LB 0x340 -> off=0x0 cb=000000000000004b uValue=00000000000c42cd 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc42cd LB 0x4b
    push bp                                   ; 55                          ; 0xc42cd vbe.c:539
    mov bp, sp                                ; 89 e5                       ; 0xc42ce
    push bx                                   ; 53                          ; 0xc42d0
    push cx                                   ; 51                          ; 0xc42d1
    push si                                   ; 56                          ; 0xc42d2
    mov si, ax                                ; 89 c6                       ; 0xc42d3
    mov bx, dx                                ; 89 d3                       ; 0xc42d5
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc42d7 vbe.c:543
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc42da
    out DX, ax                                ; ef                          ; 0xc42dd
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc42de vbe.c:544
    in ax, DX                                 ; ed                          ; 0xc42e1
    mov es, si                                ; 8e c6                       ; 0xc42e2 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc42e4
    inc bx                                    ; 43                          ; 0xc42e7 vbe.c:546
    inc bx                                    ; 43                          ; 0xc42e8
    test AL, strict byte 001h                 ; a8 01                       ; 0xc42e9 vbe.c:547
    je short 04310h                           ; 74 23                       ; 0xc42eb
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc42ed vbe.c:549
    jmp short 042f7h                          ; eb 05                       ; 0xc42f0
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc42f2
    jnbe short 04310h                         ; 77 19                       ; 0xc42f5
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc42f7 vbe.c:550
    je short 0430dh                           ; 74 11                       ; 0xc42fa
    mov ax, cx                                ; 89 c8                       ; 0xc42fc vbe.c:551
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc42fe
    out DX, ax                                ; ef                          ; 0xc4301
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4302 vbe.c:552
    in ax, DX                                 ; ed                          ; 0xc4305
    mov es, si                                ; 8e c6                       ; 0xc4306 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4308
    inc bx                                    ; 43                          ; 0xc430b vbe.c:553
    inc bx                                    ; 43                          ; 0xc430c
    inc cx                                    ; 41                          ; 0xc430d vbe.c:555
    jmp short 042f2h                          ; eb e2                       ; 0xc430e
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4310 vbe.c:556
    pop si                                    ; 5e                          ; 0xc4313
    pop cx                                    ; 59                          ; 0xc4314
    pop bx                                    ; 5b                          ; 0xc4315
    pop bp                                    ; 5d                          ; 0xc4316
    retn                                      ; c3                          ; 0xc4317
  ; disGetNextSymbol 0xc4318 LB 0x2f5 -> off=0x0 cb=000000000000008f uValue=00000000000c4318 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc4318 LB 0x8f
    push bp                                   ; 55                          ; 0xc4318 vbe.c:559
    mov bp, sp                                ; 89 e5                       ; 0xc4319
    push bx                                   ; 53                          ; 0xc431b
    push cx                                   ; 51                          ; 0xc431c
    push si                                   ; 56                          ; 0xc431d
    push ax                                   ; 50                          ; 0xc431e
    mov cx, ax                                ; 89 c1                       ; 0xc431f
    mov bx, dx                                ; 89 d3                       ; 0xc4321
    mov es, ax                                ; 8e c0                       ; 0xc4323 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4325
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc4328
    inc bx                                    ; 43                          ; 0xc432b vbe.c:564
    inc bx                                    ; 43                          ; 0xc432c
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc432d vbe.c:566
    jne short 04343h                          ; 75 10                       ; 0xc4331
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc4333 vbe.c:567
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4336
    out DX, ax                                ; ef                          ; 0xc4339
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc433a vbe.c:568
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc433d
    out DX, ax                                ; ef                          ; 0xc4340
    jmp short 0439fh                          ; eb 5c                       ; 0xc4341 vbe.c:569
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc4343 vbe.c:570
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4346
    out DX, ax                                ; ef                          ; 0xc4349
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc434a vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc434d vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4350
    inc bx                                    ; 43                          ; 0xc4351 vbe.c:572
    inc bx                                    ; 43                          ; 0xc4352
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc4353
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4356
    out DX, ax                                ; ef                          ; 0xc4359
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc435a vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc435d vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4360
    inc bx                                    ; 43                          ; 0xc4361 vbe.c:575
    inc bx                                    ; 43                          ; 0xc4362
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc4363
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4366
    out DX, ax                                ; ef                          ; 0xc4369
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc436a vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc436d vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4370
    inc bx                                    ; 43                          ; 0xc4371 vbe.c:578
    inc bx                                    ; 43                          ; 0xc4372
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc4373
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4376
    out DX, ax                                ; ef                          ; 0xc4379
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc437a vbe.c:580
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc437d
    out DX, ax                                ; ef                          ; 0xc4380
    mov si, strict word 00005h                ; be 05 00                    ; 0xc4381 vbe.c:582
    jmp short 0438bh                          ; eb 05                       ; 0xc4384
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc4386
    jnbe short 0439fh                         ; 77 14                       ; 0xc4389
    mov ax, si                                ; 89 f0                       ; 0xc438b vbe.c:583
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc438d
    out DX, ax                                ; ef                          ; 0xc4390
    mov es, cx                                ; 8e c1                       ; 0xc4391 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4393
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4396 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4399
    inc bx                                    ; 43                          ; 0xc439a vbe.c:585
    inc bx                                    ; 43                          ; 0xc439b
    inc si                                    ; 46                          ; 0xc439c vbe.c:586
    jmp short 04386h                          ; eb e7                       ; 0xc439d
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc439f vbe.c:588
    pop si                                    ; 5e                          ; 0xc43a2
    pop cx                                    ; 59                          ; 0xc43a3
    pop bx                                    ; 5b                          ; 0xc43a4
    pop bp                                    ; 5d                          ; 0xc43a5
    retn                                      ; c3                          ; 0xc43a6
  ; disGetNextSymbol 0xc43a7 LB 0x266 -> off=0x0 cb=000000000000008d uValue=00000000000c43a7 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc43a7 LB 0x8d
    push bp                                   ; 55                          ; 0xc43a7 vbe.c:604
    mov bp, sp                                ; 89 e5                       ; 0xc43a8
    push si                                   ; 56                          ; 0xc43aa
    push di                                   ; 57                          ; 0xc43ab
    push ax                                   ; 50                          ; 0xc43ac
    mov si, ax                                ; 89 c6                       ; 0xc43ad
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc43af
    mov ax, bx                                ; 89 d8                       ; 0xc43b2
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc43b4
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc43b7 vbe.c:609
    xor ah, ah                                ; 30 e4                       ; 0xc43ba vbe.c:610
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc43bc
    je short 04407h                           ; 74 46                       ; 0xc43bf
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc43c1
    je short 043ebh                           ; 74 25                       ; 0xc43c4
    test ax, ax                               ; 85 c0                       ; 0xc43c6
    jne short 04423h                          ; 75 59                       ; 0xc43c8
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc43ca vbe.c:612
    call 03370h                               ; e8 a0 ef                    ; 0xc43cd
    mov cx, ax                                ; 89 c1                       ; 0xc43d0
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc43d2 vbe.c:616
    je short 043ddh                           ; 74 05                       ; 0xc43d6
    call 042c5h                               ; e8 ea fe                    ; 0xc43d8 vbe.c:617
    add ax, cx                                ; 01 c8                       ; 0xc43db
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc43dd vbe.c:618
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc43e0
    shr ax, CL                                ; d3 e8                       ; 0xc43e2
    push SS                                   ; 16                          ; 0xc43e4
    pop ES                                    ; 07                          ; 0xc43e5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc43e6
    jmp short 04426h                          ; eb 3b                       ; 0xc43e9 vbe.c:619
    push SS                                   ; 16                          ; 0xc43eb vbe.c:621
    pop ES                                    ; 07                          ; 0xc43ec
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc43ed
    mov dx, cx                                ; 89 ca                       ; 0xc43f0 vbe.c:622
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc43f2
    call 033aeh                               ; e8 b6 ef                    ; 0xc43f5
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc43f8 vbe.c:626
    je short 04426h                           ; 74 28                       ; 0xc43fc
    mov dx, ax                                ; 89 c2                       ; 0xc43fe vbe.c:627
    mov ax, cx                                ; 89 c8                       ; 0xc4400
    call 042cdh                               ; e8 c8 fe                    ; 0xc4402
    jmp short 04426h                          ; eb 1f                       ; 0xc4405 vbe.c:628
    push SS                                   ; 16                          ; 0xc4407 vbe.c:630
    pop ES                                    ; 07                          ; 0xc4408
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4409
    mov dx, cx                                ; 89 ca                       ; 0xc440c vbe.c:631
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc440e
    call 03686h                               ; e8 72 f2                    ; 0xc4411
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4414 vbe.c:635
    je short 04426h                           ; 74 0c                       ; 0xc4418
    mov dx, ax                                ; 89 c2                       ; 0xc441a vbe.c:636
    mov ax, cx                                ; 89 c8                       ; 0xc441c
    call 04318h                               ; e8 f7 fe                    ; 0xc441e
    jmp short 04426h                          ; eb 03                       ; 0xc4421 vbe.c:637
    mov di, 00100h                            ; bf 00 01                    ; 0xc4423 vbe.c:640
    push SS                                   ; 16                          ; 0xc4426 vbe.c:643
    pop ES                                    ; 07                          ; 0xc4427
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc4428
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc442b vbe.c:644
    pop di                                    ; 5f                          ; 0xc442e
    pop si                                    ; 5e                          ; 0xc442f
    pop bp                                    ; 5d                          ; 0xc4430
    retn 00002h                               ; c2 02 00                    ; 0xc4431
  ; disGetNextSymbol 0xc4434 LB 0x1d9 -> off=0x0 cb=00000000000000e2 uValue=00000000000c4434 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc4434 LB 0xe2
    push bp                                   ; 55                          ; 0xc4434 vbe.c:665
    mov bp, sp                                ; 89 e5                       ; 0xc4435
    push si                                   ; 56                          ; 0xc4437
    push di                                   ; 57                          ; 0xc4438
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc4439
    push ax                                   ; 50                          ; 0xc443c
    mov di, dx                                ; 89 d7                       ; 0xc443d
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc443f
    mov si, cx                                ; 89 ce                       ; 0xc4442
    call 03ec0h                               ; e8 79 fa                    ; 0xc4444 vbe.c:674
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc4447 vbe.c:675
    jne short 04450h                          ; 75 05                       ; 0xc4449
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc444b
    jmp short 04454h                          ; eb 04                       ; 0xc444e
    xor ah, ah                                ; 30 e4                       ; 0xc4450
    mov cx, ax                                ; 89 c1                       ; 0xc4452
    mov ch, cl                                ; 88 cd                       ; 0xc4454
    call 03ef8h                               ; e8 9f fa                    ; 0xc4456 vbe.c:676
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc4459
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc445c vbe.c:677
    push SS                                   ; 16                          ; 0xc4461 vbe.c:678
    pop ES                                    ; 07                          ; 0xc4462
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc4463
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4466
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc4469 vbe.c:679
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc446c vbe.c:683
    je short 0447bh                           ; 74 0b                       ; 0xc446e
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc4470
    je short 044a4h                           ; 74 30                       ; 0xc4472
    test al, al                               ; 84 c0                       ; 0xc4474
    je short 0449fh                           ; 74 27                       ; 0xc4476
    jmp near 044ffh                           ; e9 84 00                    ; 0xc4478
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc447b vbe.c:685
    jne short 04486h                          ; 75 06                       ; 0xc447e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4480 vbe.c:686
    sal bx, CL                                ; d3 e3                       ; 0xc4482
    jmp short 0449fh                          ; eb 19                       ; 0xc4484 vbe.c:687
    mov al, ch                                ; 88 e8                       ; 0xc4486 vbe.c:688
    xor ah, ah                                ; 30 e4                       ; 0xc4488
    cwd                                       ; 99                          ; 0xc448a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc448b
    sal dx, CL                                ; d3 e2                       ; 0xc448d
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc448f
    sar ax, CL                                ; d3 f8                       ; 0xc4491
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc4493
    mov ax, bx                                ; 89 d8                       ; 0xc4496
    xor dx, dx                                ; 31 d2                       ; 0xc4498
    div word [bp-00eh]                        ; f7 76 f2                    ; 0xc449a
    mov bx, ax                                ; 89 c3                       ; 0xc449d
    mov ax, bx                                ; 89 d8                       ; 0xc449f vbe.c:691
    call 03ed9h                               ; e8 35 fa                    ; 0xc44a1
    call 03ef8h                               ; e8 51 fa                    ; 0xc44a4 vbe.c:694
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc44a7
    push SS                                   ; 16                          ; 0xc44aa vbe.c:695
    pop ES                                    ; 07                          ; 0xc44ab
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc44ac
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc44af
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc44b2 vbe.c:696
    jne short 044bfh                          ; 75 08                       ; 0xc44b5
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc44b7 vbe.c:697
    mov bx, ax                                ; 89 c3                       ; 0xc44b9
    shr bx, CL                                ; d3 eb                       ; 0xc44bb
    jmp short 044d5h                          ; eb 16                       ; 0xc44bd vbe.c:698
    mov al, ch                                ; 88 e8                       ; 0xc44bf vbe.c:699
    xor ah, ah                                ; 30 e4                       ; 0xc44c1
    cwd                                       ; 99                          ; 0xc44c3
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc44c4
    sal dx, CL                                ; d3 e2                       ; 0xc44c6
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc44c8
    sar ax, CL                                ; d3 f8                       ; 0xc44ca
    mov bx, ax                                ; 89 c3                       ; 0xc44cc
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc44ce
    mul bx                                    ; f7 e3                       ; 0xc44d1
    mov bx, ax                                ; 89 c3                       ; 0xc44d3
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc44d5 vbe.c:700
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc44d8
    push SS                                   ; 16                          ; 0xc44db vbe.c:701
    pop ES                                    ; 07                          ; 0xc44dc
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc44dd
    call 03f11h                               ; e8 2e fa                    ; 0xc44e0 vbe.c:702
    push SS                                   ; 16                          ; 0xc44e3
    pop ES                                    ; 07                          ; 0xc44e4
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc44e5
    call 03e88h                               ; e8 9d f9                    ; 0xc44e8 vbe.c:703
    push SS                                   ; 16                          ; 0xc44eb
    pop ES                                    ; 07                          ; 0xc44ec
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc44ed
    jbe short 04504h                          ; 76 12                       ; 0xc44f0
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc44f2 vbe.c:704
    call 03ed9h                               ; e8 e1 f9                    ; 0xc44f5
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc44f8 vbe.c:705
    jmp short 04504h                          ; eb 05                       ; 0xc44fd vbe.c:707
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc44ff vbe.c:710
    push SS                                   ; 16                          ; 0xc4504 vbe.c:713
    pop ES                                    ; 07                          ; 0xc4505
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc4506
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc4509
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc450c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc450f vbe.c:714
    pop di                                    ; 5f                          ; 0xc4512
    pop si                                    ; 5e                          ; 0xc4513
    pop bp                                    ; 5d                          ; 0xc4514
    retn                                      ; c3                          ; 0xc4515
  ; disGetNextSymbol 0xc4516 LB 0xf7 -> off=0x0 cb=00000000000000f7 uValue=00000000000c4516 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc4516 LB 0xf7
    push bp                                   ; 55                          ; 0xc4516 vbe.c:740
    mov bp, sp                                ; 89 e5                       ; 0xc4517
    push si                                   ; 56                          ; 0xc4519
    push di                                   ; 57                          ; 0xc451a
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc451b
    push ax                                   ; 50                          ; 0xc451e
    mov si, dx                                ; 89 d6                       ; 0xc451f
    mov di, cx                                ; 89 cf                       ; 0xc4521
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc4523 vbe.c:753
    push SS                                   ; 16                          ; 0xc4528 vbe.c:754
    pop ES                                    ; 07                          ; 0xc4529
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc452a
    test al, al                               ; 84 c0                       ; 0xc452d vbe.c:755
    jne short 04551h                          ; 75 20                       ; 0xc452f
    push SS                                   ; 16                          ; 0xc4531 vbe.c:757
    pop ES                                    ; 07                          ; 0xc4532
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4533
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc4536 vbe.c:758
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc4539
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc453c vbe.c:759
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc4540
    mov ch, al                                ; 88 c5                       ; 0xc4543
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc4545 vbe.c:764
    je short 04559h                           ; 74 10                       ; 0xc4547
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc4549
    je short 04559h                           ; 74 0c                       ; 0xc454b
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc454d
    je short 04559h                           ; 74 08                       ; 0xc454f
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc4551 vbe.c:765
    jmp near 045fbh                           ; e9 a2 00                    ; 0xc4556 vbe.c:766
    push SS                                   ; 16                          ; 0xc4559 vbe.c:770
    pop ES                                    ; 07                          ; 0xc455a
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc455b
    je short 04567h                           ; 74 05                       ; 0xc4560
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc4562
    jmp short 04569h                          ; eb 02                       ; 0xc4565
    xor ax, ax                                ; 31 c0                       ; 0xc4567
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc4569
    cmp bx, 00280h                            ; 81 fb 80 02                 ; 0xc456c vbe.c:773
    jnc short 04577h                          ; 73 05                       ; 0xc4570
    mov bx, 00280h                            ; bb 80 02                    ; 0xc4572 vbe.c:774
    jmp short 04580h                          ; eb 09                       ; 0xc4575 vbe.c:775
    cmp bx, 00a00h                            ; 81 fb 00 0a                 ; 0xc4577
    jbe short 04580h                          ; 76 03                       ; 0xc457b
    mov bx, 00a00h                            ; bb 00 0a                    ; 0xc457d vbe.c:776
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc4580 vbe.c:777
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc4583
    jnc short 0458fh                          ; 73 07                       ; 0xc4586
    mov word [bp-008h], 001e0h                ; c7 46 f8 e0 01              ; 0xc4588 vbe.c:778
    jmp short 04599h                          ; eb 0a                       ; 0xc458d vbe.c:779
    cmp ax, 00780h                            ; 3d 80 07                    ; 0xc458f
    jbe short 04599h                          ; 76 05                       ; 0xc4592
    mov word [bp-008h], 00780h                ; c7 46 f8 80 07              ; 0xc4594 vbe.c:780
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4599 vbe.c:786
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc459c
    call 03f2ah                               ; e8 88 f9                    ; 0xc459f
    mov si, ax                                ; 89 c6                       ; 0xc45a2
    mov al, ch                                ; 88 e8                       ; 0xc45a4 vbe.c:789
    xor ah, ah                                ; 30 e4                       ; 0xc45a6
    cwd                                       ; 99                          ; 0xc45a8
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc45a9
    sal dx, CL                                ; d3 e2                       ; 0xc45ab
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc45ad
    sar ax, CL                                ; d3 f8                       ; 0xc45af
    mov dx, ax                                ; 89 c2                       ; 0xc45b1
    mov ax, bx                                ; 89 d8                       ; 0xc45b3
    mul dx                                    ; f7 e2                       ; 0xc45b5
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc45b7 vbe.c:790
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc45ba
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc45bc vbe.c:792
    mul dx                                    ; f7 e2                       ; 0xc45bf
    cmp dx, si                                ; 39 f2                       ; 0xc45c1 vbe.c:794
    jnbe short 045cbh                         ; 77 06                       ; 0xc45c3
    jne short 045d2h                          ; 75 0b                       ; 0xc45c5
    test ax, ax                               ; 85 c0                       ; 0xc45c7
    jbe short 045d2h                          ; 76 07                       ; 0xc45c9
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc45cb vbe.c:796
    jmp short 045fbh                          ; eb 29                       ; 0xc45d0 vbe.c:797
    xor ax, ax                                ; 31 c0                       ; 0xc45d2 vbe.c:801
    call 00610h                               ; e8 39 c0                    ; 0xc45d4
    mov al, ch                                ; 88 e8                       ; 0xc45d7 vbe.c:802
    xor ah, ah                                ; 30 e4                       ; 0xc45d9
    call 03ea1h                               ; e8 c3 f8                    ; 0xc45db
    mov ax, bx                                ; 89 d8                       ; 0xc45de vbe.c:803
    call 03e4ah                               ; e8 67 f8                    ; 0xc45e0
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc45e3 vbe.c:804
    call 03e69h                               ; e8 80 f8                    ; 0xc45e6
    xor ax, ax                                ; 31 c0                       ; 0xc45e9 vbe.c:805
    call 00636h                               ; e8 48 c0                    ; 0xc45eb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc45ee vbe.c:806
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc45f1
    xor ah, ah                                ; 30 e4                       ; 0xc45f3
    call 00610h                               ; e8 18 c0                    ; 0xc45f5
    call 00708h                               ; e8 0d c1                    ; 0xc45f8 vbe.c:807
    push SS                                   ; 16                          ; 0xc45fb vbe.c:815
    pop ES                                    ; 07                          ; 0xc45fc
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc45fd
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc4600
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4603
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4606 vbe.c:816
    pop di                                    ; 5f                          ; 0xc4609
    pop si                                    ; 5e                          ; 0xc460a
    pop bp                                    ; 5d                          ; 0xc460b
    retn                                      ; c3                          ; 0xc460c

  ; Padding 0x33 bytes at 0xc460d
  times 51 db 0

section VBE32 progbits vstart=0x4640 align=1 ; size=0x115 class=CODE group=AUTO
  ; disGetNextSymbol 0xc4640 LB 0x115 -> off=0x0 cb=0000000000000114 uValue=00000000000c0000 'vesa_pm_start'
vesa_pm_start:                               ; 0xc4640 LB 0x114
    sbb byte [bx+si], al                      ; 18 00                       ; 0xc4640
    dec di                                    ; 4f                          ; 0xc4642
    add byte [bx+si], dl                      ; 00 10                       ; 0xc4643
    add word [bx+si], cx                      ; 01 08                       ; 0xc4645
    add dh, cl                                ; 00 ce                       ; 0xc4647
    add di, cx                                ; 01 cf                       ; 0xc4649
    add di, cx                                ; 01 cf                       ; 0xc464b
    add ax, dx                                ; 01 d0                       ; 0xc464d
    add word [bp-048fdh], si                  ; 01 b6 03 b7                 ; 0xc464f
    db  003h, 0ffh
    ; add di, di                                ; 03 ff                     ; 0xc4653
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83                    ; 0xc4657
    sti                                       ; fb                          ; 0xc465a
    add byte [si+005h], dh                    ; 00 74 05                    ; 0xc465b
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66           ; 0xc465e vberom.asm:825
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc4664
    push edx                                  ; 66 52                       ; 0xc4666 vberom.asm:829
    push eax                                  ; 66 50                       ; 0xc4668 vberom.asm:830
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc466a vberom.asm:831
    add ax, 06600h                            ; 05 00 66                    ; 0xc4670
    out DX, ax                                ; ef                          ; 0xc4673
    pop eax                                   ; 66 58                       ; 0xc4674 vberom.asm:834
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4676 vberom.asm:835
    in eax, DX                                ; 66 ed                       ; 0xc467c vberom.asm:837
    pop edx                                   ; 66 5a                       ; 0xc467e vberom.asm:838
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0                  ; 0xc4680 vberom.asm:839
    jne short 0468ah                          ; 75 05                       ; 0xc4683 vberom.asm:840
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc4685 vberom.asm:841
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc468b
    retn                                      ; c3                          ; 0xc468e vberom.asm:845
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc468f vberom.asm:847
    je short 0469eh                           ; 74 0a                       ; 0xc4692 vberom.asm:848
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc4694 vberom.asm:849
    je short 046aeh                           ; 74 15                       ; 0xc4697 vberom.asm:850
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52           ; 0xc4699 vberom.asm:851
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8           ; 0xc469f vberom.asm:855
    or byte [di-005h], dh                     ; 08 75 fb                    ; 0xc46a5
    in AL, DX                                 ; ec                          ; 0xc46a8 vberom.asm:861
    test AL, strict byte 008h                 ; a8 08                       ; 0xc46a9 vberom.asm:862
    je short 046a8h                           ; 74 fb                       ; 0xc46ab vberom.asm:863
    pop dx                                    ; 5a                          ; 0xc46ad vberom.asm:864
    push ax                                   ; 50                          ; 0xc46ae vberom.asm:868
    push cx                                   ; 51                          ; 0xc46af vberom.asm:869
    push dx                                   ; 52                          ; 0xc46b0 vberom.asm:870
    push si                                   ; 56                          ; 0xc46b1 vberom.asm:871
    push di                                   ; 57                          ; 0xc46b2 vberom.asm:872
    sal dx, 010h                              ; c1 e2 10                    ; 0xc46b3 vberom.asm:873
    and cx, strict word 0ffffh                ; 81 e1 ff ff                 ; 0xc46b6 vberom.asm:874
    add byte [bx+si], al                      ; 00 00                       ; 0xc46ba
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca                     ; 0xc46bc vberom.asm:875
    sal cx, 002h                              ; c1 e1 02                    ; 0xc46be vberom.asm:876
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc46c1 vberom.asm:877
    push ax                                   ; 50                          ; 0xc46c3 vberom.asm:878
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46c4 vberom.asm:879
    push ES                                   ; 06                          ; 0xc46ca
    add byte [bp-011h], ah                    ; 00 66 ef                    ; 0xc46cb
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46ce vberom.asm:882
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8                  ; 0xc46d4 vberom.asm:884
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46d7 vberom.asm:885
    add ax, word [bx+si]                      ; 03 00                       ; 0xc46dd
    out DX, eax                               ; 66 ef                       ; 0xc46df vberom.asm:887
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46e1 vberom.asm:888
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0                  ; 0xc46e7 vberom.asm:890
    pop ax                                    ; 58                          ; 0xc46ea vberom.asm:891
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc46eb vberom.asm:893
    je short 04707h                           ; 74 17                       ; 0xc46ee vberom.asm:894
    add si, strict byte 00007h                ; 83 c6 07                    ; 0xc46f0 vberom.asm:895
    shr si, 003h                              ; c1 ee 03                    ; 0xc46f3 vberom.asm:896
    imul cx, si                               ; 0f af ce                    ; 0xc46f6 vberom.asm:897
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46f9 vberom.asm:898
    div cx                                    ; f7 f1                       ; 0xc46fb vberom.asm:899
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46fd vberom.asm:900
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46ff vberom.asm:901
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc4701 vberom.asm:902
    div si                                    ; f7 f6                       ; 0xc4703 vberom.asm:903
    jmp short 04713h                          ; eb 0c                       ; 0xc4705 vberom.asm:904
    shr cx, 1                                 ; d1 e9                       ; 0xc4707 vberom.asm:907
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc4709 vberom.asm:908
    div cx                                    ; f7 f1                       ; 0xc470b vberom.asm:909
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc470d vberom.asm:910
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc470f vberom.asm:911
    sal ax, 1                                 ; d1 e0                       ; 0xc4711 vberom.asm:912
    push edx                                  ; 66 52                       ; 0xc4713 vberom.asm:915
    push eax                                  ; 66 50                       ; 0xc4715 vberom.asm:916
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4717 vberom.asm:917
    or byte [bx+si], al                       ; 08 00                       ; 0xc471d
    out DX, eax                               ; 66 ef                       ; 0xc471f vberom.asm:919
    pop eax                                   ; 66 58                       ; 0xc4721 vberom.asm:920
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4723 vberom.asm:921
    pop edx                                   ; 66 5a                       ; 0xc4729 vberom.asm:923
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7                  ; 0xc472b vberom.asm:925
    push edx                                  ; 66 52                       ; 0xc472e vberom.asm:926
    push eax                                  ; 66 50                       ; 0xc4730 vberom.asm:927
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4732 vberom.asm:928
    or word [bx+si], ax                       ; 09 00                       ; 0xc4738
    out DX, eax                               ; 66 ef                       ; 0xc473a vberom.asm:930
    pop eax                                   ; 66 58                       ; 0xc473c vberom.asm:931
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc473e vberom.asm:932
    pop edx                                   ; 66 5a                       ; 0xc4744 vberom.asm:934
    pop di                                    ; 5f                          ; 0xc4746 vberom.asm:936
    pop si                                    ; 5e                          ; 0xc4747 vberom.asm:937
    pop dx                                    ; 5a                          ; 0xc4748 vberom.asm:938
    pop cx                                    ; 59                          ; 0xc4749 vberom.asm:939
    pop ax                                    ; 58                          ; 0xc474a vberom.asm:940
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc474b vberom.asm:941
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc4751
  ; disGetNextSymbol 0xc4754 LB 0x1 -> off=0x0 cb=0000000000000001 uValue=0000000000000114 'vesa_pm_end'
vesa_pm_end:                                 ; 0xc4754 LB 0x1
    retn                                      ; c3                          ; 0xc4754 vberom.asm:946

  ; Padding 0x2b bytes at 0xc4755
  times 43 db 0

section _DATA progbits vstart=0x4780 align=1 ; size=0x374c class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4780 LB 0x374c -> off=0x0 cb=000000000000002e uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4780 LB 0x2e
    db  'Oracle VM VirtualBox Version 7.0.0 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc47ae LB 0x371e -> off=0x0 cb=0000000000000080 uValue=00000000000c002e 'vga_modes'
vga_modes:                                   ; 0xc47ae LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc482e LB 0x369e -> off=0x0 cb=0000000000000010 uValue=00000000000c00ae 'line_to_vpti'
line_to_vpti:                                ; 0xc482e LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc483e LB 0x368e -> off=0x0 cb=0000000000000004 uValue=00000000000c00be 'dac_regs'
dac_regs:                                    ; 0xc483e LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc4842 LB 0x368a -> off=0x0 cb=0000000000000780 uValue=00000000000c00c2 'video_param_table'
video_param_table:                           ; 0xc4842 LB 0x780
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 008h, 000h, 010h, 001h, 003h, 000h, 002h, 063h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 008h, 000h, 010h, 001h, 003h, 000h, 002h, 063h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
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
    db  028h, 018h, 00eh, 000h, 008h, 009h, 003h, 000h, 002h, 0a3h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 014h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 00eh, 000h, 008h, 009h, 003h, 000h, 002h, 0a3h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 014h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 00eh, 000h, 010h, 001h, 003h, 000h, 002h, 0a3h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 00eh, 000h, 010h, 001h, 003h, 000h, 002h, 0a3h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
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
  ; disGetNextSymbol 0xc4fc2 LB 0x2f0a -> off=0x0 cb=00000000000000c0 uValue=00000000000c0842 'palette0'
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
  ; disGetNextSymbol 0xc5082 LB 0x2e4a -> off=0x0 cb=00000000000000c0 uValue=00000000000c0902 'palette1'
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
  ; disGetNextSymbol 0xc5142 LB 0x2d8a -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c2 'palette2'
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
  ; disGetNextSymbol 0xc5202 LB 0x2cca -> off=0x0 cb=0000000000000300 uValue=00000000000c0a82 'palette3'
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
  ; disGetNextSymbol 0xc5502 LB 0x29ca -> off=0x0 cb=0000000000000010 uValue=00000000000c0d82 'static_functionality'
static_functionality:                        ; 0xc5502 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5512 LB 0x29ba -> off=0x0 cb=0000000000000024 uValue=00000000000c0d92 '_dcc_table'
_dcc_table:                                  ; 0xc5512 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5536 LB 0x2996 -> off=0x0 cb=000000000000001a uValue=00000000000c0db6 '_secondary_save_area'
_secondary_save_area:                        ; 0xc5536 LB 0x1a
    db  01ah, 000h, 012h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5550 LB 0x297c -> off=0x0 cb=000000000000001c uValue=00000000000c0dd0 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc5550 LB 0x1c
    db  042h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc556c LB 0x2960 -> off=0x0 cb=0000000000000800 uValue=00000000000c0dec 'vgafont8'
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
  ; disGetNextSymbol 0xc5d6c LB 0x2160 -> off=0x0 cb=0000000000000e00 uValue=00000000000c15ec 'vgafont14'
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
  ; disGetNextSymbol 0xc6b6c LB 0x1360 -> off=0x0 cb=0000000000001000 uValue=00000000000c23ec 'vgafont16'
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
  ; disGetNextSymbol 0xc7b6c LB 0x360 -> off=0x0 cb=000000000000012d uValue=00000000000c33ec 'vgafont14alt'
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
  ; disGetNextSymbol 0xc7c99 LB 0x233 -> off=0x0 cb=0000000000000144 uValue=00000000000c3519 'vgafont16alt'
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
  ; disGetNextSymbol 0xc7ddd LB 0xef -> off=0x0 cb=0000000000000008 uValue=00000000000c365d '_cga_msr'
_cga_msr:                                    ; 0xc7ddd LB 0x8
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h
  ; disGetNextSymbol 0xc7de5 LB 0xe7 -> off=0x0 cb=0000000000000008 uValue=00000000000c3665 'line_to_vpti_200'
line_to_vpti_200:                            ; 0xc7de5 LB 0x8
    db  000h, 001h, 002h, 003h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7ded LB 0xdf -> off=0x0 cb=0000000000000008 uValue=00000000000c366d 'line_to_vpti_350'
line_to_vpti_350:                            ; 0xc7ded LB 0x8
    db  013h, 014h, 015h, 016h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7df5 LB 0xd7 -> off=0x0 cb=0000000000000008 uValue=00000000000c3675 'line_to_vpti_400'
line_to_vpti_400:                            ; 0xc7df5 LB 0x8
    db  017h, 017h, 018h, 018h, 0ffh, 0ffh, 0ffh, 019h
  ; disGetNextSymbol 0xc7dfd LB 0xcf -> off=0x0 cb=0000000000000005 uValue=00000000000c367d 'row_tbl'
row_tbl:                                     ; 0xc7dfd LB 0x5
    db  000h, 00eh, 019h, 02bh, 000h
  ; disGetNextSymbol 0xc7e02 LB 0xca -> off=0x0 cb=0000000000000015 uValue=00000000000c3682 '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7e02 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7e17 LB 0xb5 -> off=0x0 cb=000000000000001d uValue=00000000000c3697 '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7e17 LB 0x1d
    db  'Oracle and/or its affiliates', 000h
  ; disGetNextSymbol 0xc7e34 LB 0x98 -> off=0x0 cb=0000000000000021 uValue=00000000000c36b4 '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7e34 LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7e55 LB 0x77 -> off=0x0 cb=0000000000000023 uValue=00000000000c36d5 '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7e55 LB 0x23
    db  'Oracle VM VirtualBox Version 7.0.0', 000h
  ; disGetNextSymbol 0xc7e78 LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c36f8 '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7e78 LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7ea3 LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c3723 '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7ea3 LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7ecc align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7ecc align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x134 bytes at 0xc7ecc
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 02fh, 068h, 06fh, 06dh
    db  065h, 02fh, 067h, 061h, 06ch, 069h, 074h, 073h, 079h, 06eh, 02fh, 063h, 06fh, 06dh, 070h, 069h
    db  06ch, 065h, 02dh, 063h, 061h, 063h, 068h, 065h, 02fh, 076h, 062h, 06fh, 078h, 02dh, 063h, 06ch
    db  065h, 061h, 06eh, 02fh, 074h, 072h, 075h, 06eh, 06bh, 02fh, 06fh, 075h, 074h, 02fh, 06ch, 069h
    db  06eh, 075h, 078h, 02eh, 061h, 06dh, 064h, 036h, 034h, 02fh, 072h, 065h, 06ch, 065h, 061h, 073h
    db  065h, 02fh, 06fh, 062h, 06ah, 02fh, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h, 06fh
    db  073h, 038h, 030h, 038h, 036h, 02fh, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h, 06fh
    db  073h, 038h, 030h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 002h
