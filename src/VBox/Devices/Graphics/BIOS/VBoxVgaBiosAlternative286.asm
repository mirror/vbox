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





section VGAROM progbits vstart=0x0 align=1 ; size=0x8fd class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x8fd -> off=0x22 cb=000000000000054e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 063h, 00ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x54e
    pushfw                                    ; 9c                          ; 0xc0022 vgarom.asm:84
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0023 vgarom.asm:96
    jne short 0002eh                          ; 75 06                       ; 0xc0026 vgarom.asm:97
    call 0017ah                               ; e8 4f 01                    ; 0xc0028 vgarom.asm:98
    jmp near 000eah                           ; e9 bc 00                    ; 0xc002b vgarom.asm:99
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc002e vgarom.asm:101
    jne short 00039h                          ; 75 06                       ; 0xc0031 vgarom.asm:102
    call 0052fh                               ; e8 f9 04                    ; 0xc0033 vgarom.asm:103
    jmp near 000eah                           ; e9 b1 00                    ; 0xc0036 vgarom.asm:104
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc0039 vgarom.asm:106
    jne short 00044h                          ; 75 06                       ; 0xc003c vgarom.asm:107
    call 000ech                               ; e8 ab 00                    ; 0xc003e vgarom.asm:108
    jmp near 000eah                           ; e9 a6 00                    ; 0xc0041 vgarom.asm:109
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc0044 vgarom.asm:111
    jne short 0004fh                          ; 75 06                       ; 0xc0047 vgarom.asm:112
    call 00426h                               ; e8 da 03                    ; 0xc0049 vgarom.asm:113
    jmp near 000eah                           ; e9 9b 00                    ; 0xc004c vgarom.asm:114
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc004f vgarom.asm:116
    jne short 00092h                          ; 75 3e                       ; 0xc0052 vgarom.asm:117
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc0054 vgarom.asm:118
    jne short 0005fh                          ; 75 06                       ; 0xc0057 vgarom.asm:119
    call 00433h                               ; e8 d7 03                    ; 0xc0059 vgarom.asm:120
    jmp near 000eah                           ; e9 8b 00                    ; 0xc005c vgarom.asm:121
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc005f vgarom.asm:123
    jne short 0006ah                          ; 75 06                       ; 0xc0062 vgarom.asm:124
    call 00456h                               ; e8 ef 03                    ; 0xc0064 vgarom.asm:125
    jmp near 000eah                           ; e9 80 00                    ; 0xc0067 vgarom.asm:126
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc006a vgarom.asm:128
    jne short 00074h                          ; 75 05                       ; 0xc006d vgarom.asm:129
    call 004a9h                               ; e8 37 04                    ; 0xc006f vgarom.asm:130
    jmp short 000eah                          ; eb 76                       ; 0xc0072 vgarom.asm:131
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0074 vgarom.asm:133
    jne short 0007eh                          ; 75 05                       ; 0xc0077 vgarom.asm:134
    call 004cbh                               ; e8 4f 04                    ; 0xc0079 vgarom.asm:135
    jmp short 000eah                          ; eb 6c                       ; 0xc007c vgarom.asm:136
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc007e vgarom.asm:138
    jne short 00088h                          ; 75 05                       ; 0xc0081 vgarom.asm:139
    call 004e9h                               ; e8 63 04                    ; 0xc0083 vgarom.asm:140
    jmp short 000eah                          ; eb 62                       ; 0xc0086 vgarom.asm:141
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc0088 vgarom.asm:143
    jne short 000dch                          ; 75 4f                       ; 0xc008b vgarom.asm:144
    call 0050dh                               ; e8 7d 04                    ; 0xc008d vgarom.asm:145
    jmp short 000eah                          ; eb 58                       ; 0xc0090 vgarom.asm:146
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0092 vgarom.asm:148
    je short 000dch                           ; 74 45                       ; 0xc0095 vgarom.asm:149
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc0097 vgarom.asm:150
    jne short 000a1h                          ; 75 05                       ; 0xc009a vgarom.asm:154
    call 001a1h                               ; e8 02 01                    ; 0xc009c vgarom.asm:156
    jmp short 000eah                          ; eb 49                       ; 0xc009f vgarom.asm:157
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a1 vgarom.asm:160
    jne short 000dch                          ; 75 36                       ; 0xc00a4 vgarom.asm:161
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00a6 vgarom.asm:162
    jne short 000afh                          ; 75 05                       ; 0xc00a8 vgarom.asm:163
    call 007d2h                               ; e8 25 07                    ; 0xc00aa vgarom.asm:164
    jmp short 000eah                          ; eb 3b                       ; 0xc00ad vgarom.asm:165
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00af vgarom.asm:167
    jne short 000b8h                          ; 75 05                       ; 0xc00b1 vgarom.asm:168
    call 007f7h                               ; e8 41 07                    ; 0xc00b3 vgarom.asm:169
    jmp short 000eah                          ; eb 32                       ; 0xc00b6 vgarom.asm:170
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00b8 vgarom.asm:172
    jne short 000c1h                          ; 75 05                       ; 0xc00ba vgarom.asm:173
    call 00824h                               ; e8 65 07                    ; 0xc00bc vgarom.asm:174
    jmp short 000eah                          ; eb 29                       ; 0xc00bf vgarom.asm:175
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c1 vgarom.asm:177
    jne short 000cah                          ; 75 05                       ; 0xc00c3 vgarom.asm:178
    call 00858h                               ; e8 90 07                    ; 0xc00c5 vgarom.asm:179
    jmp short 000eah                          ; eb 20                       ; 0xc00c8 vgarom.asm:180
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00ca vgarom.asm:182
    jne short 000d3h                          ; 75 05                       ; 0xc00cc vgarom.asm:183
    call 0088fh                               ; e8 be 07                    ; 0xc00ce vgarom.asm:184
    jmp short 000eah                          ; eb 17                       ; 0xc00d1 vgarom.asm:185
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d3 vgarom.asm:187
    jne short 000dch                          ; 75 05                       ; 0xc00d5 vgarom.asm:188
    call 008e6h                               ; e8 0c 08                    ; 0xc00d7 vgarom.asm:189
    jmp short 000eah                          ; eb 0e                       ; 0xc00da vgarom.asm:190
    push ES                                   ; 06                          ; 0xc00dc vgarom.asm:194
    push DS                                   ; 1e                          ; 0xc00dd vgarom.asm:195
    pushaw                                    ; 60                          ; 0xc00de vgarom.asm:97
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc00df vgarom.asm:199
    mov ds, bx                                ; 8e db                       ; 0xc00e2 vgarom.asm:200
    call 03268h                               ; e8 81 31                    ; 0xc00e4 vgarom.asm:201
    popaw                                     ; 61                          ; 0xc00e7 vgarom.asm:114
    pop DS                                    ; 1f                          ; 0xc00e8 vgarom.asm:204
    pop ES                                    ; 07                          ; 0xc00e9 vgarom.asm:205
    popfw                                     ; 9d                          ; 0xc00ea vgarom.asm:207
    iret                                      ; cf                          ; 0xc00eb vgarom.asm:208
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00ec vgarom.asm:213
    je short 000f7h                           ; 74 06                       ; 0xc00ef vgarom.asm:214
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc00f1 vgarom.asm:215
    je short 00148h                           ; 74 52                       ; 0xc00f4 vgarom.asm:216
    retn                                      ; c3                          ; 0xc00f6 vgarom.asm:220
    push ax                                   ; 50                          ; 0xc00f7 vgarom.asm:222
    push bx                                   ; 53                          ; 0xc00f8 vgarom.asm:223
    push cx                                   ; 51                          ; 0xc00f9 vgarom.asm:224
    push dx                                   ; 52                          ; 0xc00fa vgarom.asm:225
    push DS                                   ; 1e                          ; 0xc00fb vgarom.asm:226
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc00fc vgarom.asm:227
    mov ds, dx                                ; 8e da                       ; 0xc00ff vgarom.asm:228
    mov dx, 003dah                            ; ba da 03                    ; 0xc0101 vgarom.asm:229
    in AL, DX                                 ; ec                          ; 0xc0104 vgarom.asm:230
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0105 vgarom.asm:231
    jbe short 0013bh                          ; 76 2f                       ; 0xc010a vgarom.asm:232
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc010c vgarom.asm:233
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc010f vgarom.asm:234
    out DX, AL                                ; ee                          ; 0xc0111 vgarom.asm:235
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0112 vgarom.asm:236
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0114 vgarom.asm:237
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0116 vgarom.asm:238
    je short 0011ch                           ; 74 02                       ; 0xc0118 vgarom.asm:239
    add AL, strict byte 008h                  ; 04 08                       ; 0xc011a vgarom.asm:240
    out DX, AL                                ; ee                          ; 0xc011c vgarom.asm:242
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc011d vgarom.asm:243
    and bl, 010h                              ; 80 e3 10                    ; 0xc011f vgarom.asm:244
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0122 vgarom.asm:246
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0125 vgarom.asm:247
    out DX, AL                                ; ee                          ; 0xc0127 vgarom.asm:248
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0128 vgarom.asm:249
    in AL, DX                                 ; ec                          ; 0xc012b vgarom.asm:250
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc012c vgarom.asm:251
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc012e vgarom.asm:252
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0130 vgarom.asm:253
    out DX, AL                                ; ee                          ; 0xc0133 vgarom.asm:254
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0134 vgarom.asm:255
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0136 vgarom.asm:256
    jne short 00122h                          ; 75 e7                       ; 0xc0139 vgarom.asm:257
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc013b vgarom.asm:259
    out DX, AL                                ; ee                          ; 0xc013d vgarom.asm:260
    mov dx, 003dah                            ; ba da 03                    ; 0xc013e vgarom.asm:262
    in AL, DX                                 ; ec                          ; 0xc0141 vgarom.asm:263
    pop DS                                    ; 1f                          ; 0xc0142 vgarom.asm:265
    pop dx                                    ; 5a                          ; 0xc0143 vgarom.asm:266
    pop cx                                    ; 59                          ; 0xc0144 vgarom.asm:267
    pop bx                                    ; 5b                          ; 0xc0145 vgarom.asm:268
    pop ax                                    ; 58                          ; 0xc0146 vgarom.asm:269
    retn                                      ; c3                          ; 0xc0147 vgarom.asm:270
    push ax                                   ; 50                          ; 0xc0148 vgarom.asm:272
    push bx                                   ; 53                          ; 0xc0149 vgarom.asm:273
    push cx                                   ; 51                          ; 0xc014a vgarom.asm:274
    push dx                                   ; 52                          ; 0xc014b vgarom.asm:275
    mov dx, 003dah                            ; ba da 03                    ; 0xc014c vgarom.asm:276
    in AL, DX                                 ; ec                          ; 0xc014f vgarom.asm:277
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0150 vgarom.asm:278
    and bl, 001h                              ; 80 e3 01                    ; 0xc0152 vgarom.asm:279
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0155 vgarom.asm:281
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0158 vgarom.asm:282
    out DX, AL                                ; ee                          ; 0xc015a vgarom.asm:283
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc015b vgarom.asm:284
    in AL, DX                                 ; ec                          ; 0xc015e vgarom.asm:285
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc015f vgarom.asm:286
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0161 vgarom.asm:287
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0163 vgarom.asm:288
    out DX, AL                                ; ee                          ; 0xc0166 vgarom.asm:289
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0167 vgarom.asm:290
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0169 vgarom.asm:291
    jne short 00155h                          ; 75 e7                       ; 0xc016c vgarom.asm:292
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc016e vgarom.asm:293
    out DX, AL                                ; ee                          ; 0xc0170 vgarom.asm:294
    mov dx, 003dah                            ; ba da 03                    ; 0xc0171 vgarom.asm:296
    in AL, DX                                 ; ec                          ; 0xc0174 vgarom.asm:297
    pop dx                                    ; 5a                          ; 0xc0175 vgarom.asm:299
    pop cx                                    ; 59                          ; 0xc0176 vgarom.asm:300
    pop bx                                    ; 5b                          ; 0xc0177 vgarom.asm:301
    pop ax                                    ; 58                          ; 0xc0178 vgarom.asm:302
    retn                                      ; c3                          ; 0xc0179 vgarom.asm:303
    push DS                                   ; 1e                          ; 0xc017a vgarom.asm:308
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc017b vgarom.asm:309
    mov ds, ax                                ; 8e d8                       ; 0xc017e vgarom.asm:310
    push bx                                   ; 53                          ; 0xc0180 vgarom.asm:311
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc0181 vgarom.asm:312
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0184 vgarom.asm:313
    pop bx                                    ; 5b                          ; 0xc0186 vgarom.asm:314
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0187 vgarom.asm:315
    push bx                                   ; 53                          ; 0xc0189 vgarom.asm:316
    mov bx, 00087h                            ; bb 87 00                    ; 0xc018a vgarom.asm:317
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc018d vgarom.asm:318
    and ah, 080h                              ; 80 e4 80                    ; 0xc018f vgarom.asm:319
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0192 vgarom.asm:320
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0195 vgarom.asm:321
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc0197 vgarom.asm:322
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0199 vgarom.asm:323
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc019c vgarom.asm:324
    pop bx                                    ; 5b                          ; 0xc019e vgarom.asm:325
    pop DS                                    ; 1f                          ; 0xc019f vgarom.asm:326
    retn                                      ; c3                          ; 0xc01a0 vgarom.asm:327
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01a1 vgarom.asm:332
    jne short 001a7h                          ; 75 02                       ; 0xc01a3 vgarom.asm:333
    jmp short 00208h                          ; eb 61                       ; 0xc01a5 vgarom.asm:334
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01a7 vgarom.asm:336
    jne short 001adh                          ; 75 02                       ; 0xc01a9 vgarom.asm:337
    jmp short 00226h                          ; eb 79                       ; 0xc01ab vgarom.asm:338
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01ad vgarom.asm:340
    jne short 001b3h                          ; 75 02                       ; 0xc01af vgarom.asm:341
    jmp short 0022eh                          ; eb 7b                       ; 0xc01b1 vgarom.asm:342
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01b3 vgarom.asm:344
    jne short 001bah                          ; 75 03                       ; 0xc01b5 vgarom.asm:345
    jmp near 0025fh                           ; e9 a5 00                    ; 0xc01b7 vgarom.asm:346
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01ba vgarom.asm:348
    jne short 001c1h                          ; 75 03                       ; 0xc01bc vgarom.asm:349
    jmp near 00289h                           ; e9 c8 00                    ; 0xc01be vgarom.asm:350
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01c1 vgarom.asm:352
    jne short 001c8h                          ; 75 03                       ; 0xc01c3 vgarom.asm:353
    jmp near 002b1h                           ; e9 e9 00                    ; 0xc01c5 vgarom.asm:354
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01c8 vgarom.asm:356
    jne short 001cfh                          ; 75 03                       ; 0xc01ca vgarom.asm:357
    jmp near 002bfh                           ; e9 f0 00                    ; 0xc01cc vgarom.asm:358
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01cf vgarom.asm:360
    jne short 001d6h                          ; 75 03                       ; 0xc01d1 vgarom.asm:361
    jmp near 00304h                           ; e9 2e 01                    ; 0xc01d3 vgarom.asm:362
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01d6 vgarom.asm:364
    jne short 001ddh                          ; 75 03                       ; 0xc01d8 vgarom.asm:365
    jmp near 0031dh                           ; e9 40 01                    ; 0xc01da vgarom.asm:366
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01dd vgarom.asm:368
    jne short 001e4h                          ; 75 03                       ; 0xc01df vgarom.asm:369
    jmp near 00345h                           ; e9 61 01                    ; 0xc01e1 vgarom.asm:370
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01e4 vgarom.asm:372
    jne short 001ebh                          ; 75 03                       ; 0xc01e6 vgarom.asm:373
    jmp near 0038ch                           ; e9 a1 01                    ; 0xc01e8 vgarom.asm:374
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01eb vgarom.asm:376
    jne short 001f2h                          ; 75 03                       ; 0xc01ed vgarom.asm:377
    jmp near 003a7h                           ; e9 b5 01                    ; 0xc01ef vgarom.asm:378
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc01f2 vgarom.asm:380
    jne short 001f9h                          ; 75 03                       ; 0xc01f4 vgarom.asm:381
    jmp near 003cfh                           ; e9 d6 01                    ; 0xc01f6 vgarom.asm:382
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc01f9 vgarom.asm:384
    jne short 00200h                          ; 75 03                       ; 0xc01fb vgarom.asm:385
    jmp near 003dah                           ; e9 da 01                    ; 0xc01fd vgarom.asm:386
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc0200 vgarom.asm:388
    jne short 00207h                          ; 75 03                       ; 0xc0202 vgarom.asm:389
    jmp near 003e5h                           ; e9 de 01                    ; 0xc0204 vgarom.asm:390
    retn                                      ; c3                          ; 0xc0207 vgarom.asm:395
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0208 vgarom.asm:398
    jnbe short 00225h                         ; 77 18                       ; 0xc020b vgarom.asm:399
    push ax                                   ; 50                          ; 0xc020d vgarom.asm:400
    push dx                                   ; 52                          ; 0xc020e vgarom.asm:401
    mov dx, 003dah                            ; ba da 03                    ; 0xc020f vgarom.asm:402
    in AL, DX                                 ; ec                          ; 0xc0212 vgarom.asm:403
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0213 vgarom.asm:404
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0216 vgarom.asm:405
    out DX, AL                                ; ee                          ; 0xc0218 vgarom.asm:406
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0219 vgarom.asm:407
    out DX, AL                                ; ee                          ; 0xc021b vgarom.asm:408
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc021c vgarom.asm:409
    out DX, AL                                ; ee                          ; 0xc021e vgarom.asm:410
    mov dx, 003dah                            ; ba da 03                    ; 0xc021f vgarom.asm:412
    in AL, DX                                 ; ec                          ; 0xc0222 vgarom.asm:413
    pop dx                                    ; 5a                          ; 0xc0223 vgarom.asm:415
    pop ax                                    ; 58                          ; 0xc0224 vgarom.asm:416
    retn                                      ; c3                          ; 0xc0225 vgarom.asm:418
    push bx                                   ; 53                          ; 0xc0226 vgarom.asm:423
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc0227 vgarom.asm:424
    call 00208h                               ; e8 dc ff                    ; 0xc0229 vgarom.asm:425
    pop bx                                    ; 5b                          ; 0xc022c vgarom.asm:426
    retn                                      ; c3                          ; 0xc022d vgarom.asm:427
    push ax                                   ; 50                          ; 0xc022e vgarom.asm:432
    push bx                                   ; 53                          ; 0xc022f vgarom.asm:433
    push cx                                   ; 51                          ; 0xc0230 vgarom.asm:434
    push dx                                   ; 52                          ; 0xc0231 vgarom.asm:435
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0232 vgarom.asm:436
    mov dx, 003dah                            ; ba da 03                    ; 0xc0234 vgarom.asm:437
    in AL, DX                                 ; ec                          ; 0xc0237 vgarom.asm:438
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc0238 vgarom.asm:439
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc023a vgarom.asm:440
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc023d vgarom.asm:442
    out DX, AL                                ; ee                          ; 0xc023f vgarom.asm:443
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0240 vgarom.asm:444
    out DX, AL                                ; ee                          ; 0xc0243 vgarom.asm:445
    inc bx                                    ; 43                          ; 0xc0244 vgarom.asm:446
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0245 vgarom.asm:447
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc0247 vgarom.asm:448
    jne short 0023dh                          ; 75 f1                       ; 0xc024a vgarom.asm:449
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc024c vgarom.asm:450
    out DX, AL                                ; ee                          ; 0xc024e vgarom.asm:451
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc024f vgarom.asm:452
    out DX, AL                                ; ee                          ; 0xc0252 vgarom.asm:453
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0253 vgarom.asm:454
    out DX, AL                                ; ee                          ; 0xc0255 vgarom.asm:455
    mov dx, 003dah                            ; ba da 03                    ; 0xc0256 vgarom.asm:457
    in AL, DX                                 ; ec                          ; 0xc0259 vgarom.asm:458
    pop dx                                    ; 5a                          ; 0xc025a vgarom.asm:460
    pop cx                                    ; 59                          ; 0xc025b vgarom.asm:461
    pop bx                                    ; 5b                          ; 0xc025c vgarom.asm:462
    pop ax                                    ; 58                          ; 0xc025d vgarom.asm:463
    retn                                      ; c3                          ; 0xc025e vgarom.asm:464
    push ax                                   ; 50                          ; 0xc025f vgarom.asm:469
    push bx                                   ; 53                          ; 0xc0260 vgarom.asm:470
    push dx                                   ; 52                          ; 0xc0261 vgarom.asm:471
    mov dx, 003dah                            ; ba da 03                    ; 0xc0262 vgarom.asm:472
    in AL, DX                                 ; ec                          ; 0xc0265 vgarom.asm:473
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0266 vgarom.asm:474
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0269 vgarom.asm:475
    out DX, AL                                ; ee                          ; 0xc026b vgarom.asm:476
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc026c vgarom.asm:477
    in AL, DX                                 ; ec                          ; 0xc026f vgarom.asm:478
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc0270 vgarom.asm:479
    and bl, 001h                              ; 80 e3 01                    ; 0xc0272 vgarom.asm:480
    sal bl, 003h                              ; c0 e3 03                    ; 0xc0275 vgarom.asm:482
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0278 vgarom.asm:488
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc027a vgarom.asm:489
    out DX, AL                                ; ee                          ; 0xc027d vgarom.asm:490
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc027e vgarom.asm:491
    out DX, AL                                ; ee                          ; 0xc0280 vgarom.asm:492
    mov dx, 003dah                            ; ba da 03                    ; 0xc0281 vgarom.asm:494
    in AL, DX                                 ; ec                          ; 0xc0284 vgarom.asm:495
    pop dx                                    ; 5a                          ; 0xc0285 vgarom.asm:497
    pop bx                                    ; 5b                          ; 0xc0286 vgarom.asm:498
    pop ax                                    ; 58                          ; 0xc0287 vgarom.asm:499
    retn                                      ; c3                          ; 0xc0288 vgarom.asm:500
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0289 vgarom.asm:505
    jnbe short 002b0h                         ; 77 22                       ; 0xc028c vgarom.asm:506
    push ax                                   ; 50                          ; 0xc028e vgarom.asm:507
    push dx                                   ; 52                          ; 0xc028f vgarom.asm:508
    mov dx, 003dah                            ; ba da 03                    ; 0xc0290 vgarom.asm:509
    in AL, DX                                 ; ec                          ; 0xc0293 vgarom.asm:510
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0294 vgarom.asm:511
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0297 vgarom.asm:512
    out DX, AL                                ; ee                          ; 0xc0299 vgarom.asm:513
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc029a vgarom.asm:514
    in AL, DX                                 ; ec                          ; 0xc029d vgarom.asm:515
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc029e vgarom.asm:516
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a0 vgarom.asm:517
    in AL, DX                                 ; ec                          ; 0xc02a3 vgarom.asm:518
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a4 vgarom.asm:519
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02a7 vgarom.asm:520
    out DX, AL                                ; ee                          ; 0xc02a9 vgarom.asm:521
    mov dx, 003dah                            ; ba da 03                    ; 0xc02aa vgarom.asm:523
    in AL, DX                                 ; ec                          ; 0xc02ad vgarom.asm:524
    pop dx                                    ; 5a                          ; 0xc02ae vgarom.asm:526
    pop ax                                    ; 58                          ; 0xc02af vgarom.asm:527
    retn                                      ; c3                          ; 0xc02b0 vgarom.asm:529
    push ax                                   ; 50                          ; 0xc02b1 vgarom.asm:534
    push bx                                   ; 53                          ; 0xc02b2 vgarom.asm:535
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02b3 vgarom.asm:536
    call 00289h                               ; e8 d1 ff                    ; 0xc02b5 vgarom.asm:537
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02b8 vgarom.asm:538
    pop bx                                    ; 5b                          ; 0xc02ba vgarom.asm:539
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02bb vgarom.asm:540
    pop ax                                    ; 58                          ; 0xc02bd vgarom.asm:541
    retn                                      ; c3                          ; 0xc02be vgarom.asm:542
    push ax                                   ; 50                          ; 0xc02bf vgarom.asm:547
    push bx                                   ; 53                          ; 0xc02c0 vgarom.asm:548
    push cx                                   ; 51                          ; 0xc02c1 vgarom.asm:549
    push dx                                   ; 52                          ; 0xc02c2 vgarom.asm:550
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02c3 vgarom.asm:551
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02c5 vgarom.asm:552
    mov dx, 003dah                            ; ba da 03                    ; 0xc02c7 vgarom.asm:554
    in AL, DX                                 ; ec                          ; 0xc02ca vgarom.asm:555
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02cb vgarom.asm:556
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02ce vgarom.asm:557
    out DX, AL                                ; ee                          ; 0xc02d0 vgarom.asm:558
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02d1 vgarom.asm:559
    in AL, DX                                 ; ec                          ; 0xc02d4 vgarom.asm:560
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02d5 vgarom.asm:561
    inc bx                                    ; 43                          ; 0xc02d8 vgarom.asm:562
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02d9 vgarom.asm:563
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02db vgarom.asm:564
    jne short 002c7h                          ; 75 e7                       ; 0xc02de vgarom.asm:565
    mov dx, 003dah                            ; ba da 03                    ; 0xc02e0 vgarom.asm:566
    in AL, DX                                 ; ec                          ; 0xc02e3 vgarom.asm:567
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02e4 vgarom.asm:568
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02e7 vgarom.asm:569
    out DX, AL                                ; ee                          ; 0xc02e9 vgarom.asm:570
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02ea vgarom.asm:571
    in AL, DX                                 ; ec                          ; 0xc02ed vgarom.asm:572
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02ee vgarom.asm:573
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f1 vgarom.asm:574
    in AL, DX                                 ; ec                          ; 0xc02f4 vgarom.asm:575
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f5 vgarom.asm:576
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02f8 vgarom.asm:577
    out DX, AL                                ; ee                          ; 0xc02fa vgarom.asm:578
    mov dx, 003dah                            ; ba da 03                    ; 0xc02fb vgarom.asm:580
    in AL, DX                                 ; ec                          ; 0xc02fe vgarom.asm:581
    pop dx                                    ; 5a                          ; 0xc02ff vgarom.asm:583
    pop cx                                    ; 59                          ; 0xc0300 vgarom.asm:584
    pop bx                                    ; 5b                          ; 0xc0301 vgarom.asm:585
    pop ax                                    ; 58                          ; 0xc0302 vgarom.asm:586
    retn                                      ; c3                          ; 0xc0303 vgarom.asm:587
    push ax                                   ; 50                          ; 0xc0304 vgarom.asm:592
    push dx                                   ; 52                          ; 0xc0305 vgarom.asm:593
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0306 vgarom.asm:594
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0309 vgarom.asm:595
    out DX, AL                                ; ee                          ; 0xc030b vgarom.asm:596
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc030c vgarom.asm:597
    pop ax                                    ; 58                          ; 0xc030f vgarom.asm:598
    push ax                                   ; 50                          ; 0xc0310 vgarom.asm:599
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0311 vgarom.asm:600
    out DX, AL                                ; ee                          ; 0xc0313 vgarom.asm:601
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0314 vgarom.asm:602
    out DX, AL                                ; ee                          ; 0xc0316 vgarom.asm:603
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0317 vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc0319 vgarom.asm:605
    pop dx                                    ; 5a                          ; 0xc031a vgarom.asm:606
    pop ax                                    ; 58                          ; 0xc031b vgarom.asm:607
    retn                                      ; c3                          ; 0xc031c vgarom.asm:608
    push ax                                   ; 50                          ; 0xc031d vgarom.asm:613
    push bx                                   ; 53                          ; 0xc031e vgarom.asm:614
    push cx                                   ; 51                          ; 0xc031f vgarom.asm:615
    push dx                                   ; 52                          ; 0xc0320 vgarom.asm:616
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0321 vgarom.asm:617
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0324 vgarom.asm:618
    out DX, AL                                ; ee                          ; 0xc0326 vgarom.asm:619
    pop dx                                    ; 5a                          ; 0xc0327 vgarom.asm:620
    push dx                                   ; 52                          ; 0xc0328 vgarom.asm:621
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0329 vgarom.asm:622
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc032b vgarom.asm:623
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc032e vgarom.asm:625
    out DX, AL                                ; ee                          ; 0xc0331 vgarom.asm:626
    inc bx                                    ; 43                          ; 0xc0332 vgarom.asm:627
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0333 vgarom.asm:628
    out DX, AL                                ; ee                          ; 0xc0336 vgarom.asm:629
    inc bx                                    ; 43                          ; 0xc0337 vgarom.asm:630
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0338 vgarom.asm:631
    out DX, AL                                ; ee                          ; 0xc033b vgarom.asm:632
    inc bx                                    ; 43                          ; 0xc033c vgarom.asm:633
    dec cx                                    ; 49                          ; 0xc033d vgarom.asm:634
    jne short 0032eh                          ; 75 ee                       ; 0xc033e vgarom.asm:635
    pop dx                                    ; 5a                          ; 0xc0340 vgarom.asm:636
    pop cx                                    ; 59                          ; 0xc0341 vgarom.asm:637
    pop bx                                    ; 5b                          ; 0xc0342 vgarom.asm:638
    pop ax                                    ; 58                          ; 0xc0343 vgarom.asm:639
    retn                                      ; c3                          ; 0xc0344 vgarom.asm:640
    push ax                                   ; 50                          ; 0xc0345 vgarom.asm:645
    push bx                                   ; 53                          ; 0xc0346 vgarom.asm:646
    push dx                                   ; 52                          ; 0xc0347 vgarom.asm:647
    mov dx, 003dah                            ; ba da 03                    ; 0xc0348 vgarom.asm:648
    in AL, DX                                 ; ec                          ; 0xc034b vgarom.asm:649
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc034c vgarom.asm:650
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc034f vgarom.asm:651
    out DX, AL                                ; ee                          ; 0xc0351 vgarom.asm:652
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0352 vgarom.asm:653
    in AL, DX                                 ; ec                          ; 0xc0355 vgarom.asm:654
    and bl, 001h                              ; 80 e3 01                    ; 0xc0356 vgarom.asm:655
    jne short 00368h                          ; 75 0d                       ; 0xc0359 vgarom.asm:656
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc035b vgarom.asm:657
    sal bh, 007h                              ; c0 e7 07                    ; 0xc035d vgarom.asm:659
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc0360 vgarom.asm:669
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0362 vgarom.asm:670
    out DX, AL                                ; ee                          ; 0xc0365 vgarom.asm:671
    jmp short 00381h                          ; eb 19                       ; 0xc0366 vgarom.asm:672
    push ax                                   ; 50                          ; 0xc0368 vgarom.asm:674
    mov dx, 003dah                            ; ba da 03                    ; 0xc0369 vgarom.asm:675
    in AL, DX                                 ; ec                          ; 0xc036c vgarom.asm:676
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc036d vgarom.asm:677
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0370 vgarom.asm:678
    out DX, AL                                ; ee                          ; 0xc0372 vgarom.asm:679
    pop ax                                    ; 58                          ; 0xc0373 vgarom.asm:680
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0374 vgarom.asm:681
    jne short 0037bh                          ; 75 03                       ; 0xc0376 vgarom.asm:682
    sal bh, 002h                              ; c0 e7 02                    ; 0xc0378 vgarom.asm:684
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc037b vgarom.asm:690
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc037e vgarom.asm:691
    out DX, AL                                ; ee                          ; 0xc0380 vgarom.asm:692
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0381 vgarom.asm:694
    out DX, AL                                ; ee                          ; 0xc0383 vgarom.asm:695
    mov dx, 003dah                            ; ba da 03                    ; 0xc0384 vgarom.asm:697
    in AL, DX                                 ; ec                          ; 0xc0387 vgarom.asm:698
    pop dx                                    ; 5a                          ; 0xc0388 vgarom.asm:700
    pop bx                                    ; 5b                          ; 0xc0389 vgarom.asm:701
    pop ax                                    ; 58                          ; 0xc038a vgarom.asm:702
    retn                                      ; c3                          ; 0xc038b vgarom.asm:703
    push ax                                   ; 50                          ; 0xc038c vgarom.asm:708
    push dx                                   ; 52                          ; 0xc038d vgarom.asm:709
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc038e vgarom.asm:710
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0391 vgarom.asm:711
    out DX, AL                                ; ee                          ; 0xc0393 vgarom.asm:712
    pop ax                                    ; 58                          ; 0xc0394 vgarom.asm:713
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0395 vgarom.asm:714
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0397 vgarom.asm:715
    in AL, DX                                 ; ec                          ; 0xc039a vgarom.asm:716
    xchg al, ah                               ; 86 e0                       ; 0xc039b vgarom.asm:717
    push ax                                   ; 50                          ; 0xc039d vgarom.asm:718
    in AL, DX                                 ; ec                          ; 0xc039e vgarom.asm:719
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc039f vgarom.asm:720
    in AL, DX                                 ; ec                          ; 0xc03a1 vgarom.asm:721
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03a2 vgarom.asm:722
    pop dx                                    ; 5a                          ; 0xc03a4 vgarom.asm:723
    pop ax                                    ; 58                          ; 0xc03a5 vgarom.asm:724
    retn                                      ; c3                          ; 0xc03a6 vgarom.asm:725
    push ax                                   ; 50                          ; 0xc03a7 vgarom.asm:730
    push bx                                   ; 53                          ; 0xc03a8 vgarom.asm:731
    push cx                                   ; 51                          ; 0xc03a9 vgarom.asm:732
    push dx                                   ; 52                          ; 0xc03aa vgarom.asm:733
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03ab vgarom.asm:734
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03ae vgarom.asm:735
    out DX, AL                                ; ee                          ; 0xc03b0 vgarom.asm:736
    pop dx                                    ; 5a                          ; 0xc03b1 vgarom.asm:737
    push dx                                   ; 52                          ; 0xc03b2 vgarom.asm:738
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03b3 vgarom.asm:739
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b5 vgarom.asm:740
    in AL, DX                                 ; ec                          ; 0xc03b8 vgarom.asm:742
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03b9 vgarom.asm:743
    inc bx                                    ; 43                          ; 0xc03bc vgarom.asm:744
    in AL, DX                                 ; ec                          ; 0xc03bd vgarom.asm:745
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03be vgarom.asm:746
    inc bx                                    ; 43                          ; 0xc03c1 vgarom.asm:747
    in AL, DX                                 ; ec                          ; 0xc03c2 vgarom.asm:748
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03c3 vgarom.asm:749
    inc bx                                    ; 43                          ; 0xc03c6 vgarom.asm:750
    dec cx                                    ; 49                          ; 0xc03c7 vgarom.asm:751
    jne short 003b8h                          ; 75 ee                       ; 0xc03c8 vgarom.asm:752
    pop dx                                    ; 5a                          ; 0xc03ca vgarom.asm:753
    pop cx                                    ; 59                          ; 0xc03cb vgarom.asm:754
    pop bx                                    ; 5b                          ; 0xc03cc vgarom.asm:755
    pop ax                                    ; 58                          ; 0xc03cd vgarom.asm:756
    retn                                      ; c3                          ; 0xc03ce vgarom.asm:757
    push ax                                   ; 50                          ; 0xc03cf vgarom.asm:762
    push dx                                   ; 52                          ; 0xc03d0 vgarom.asm:763
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03d1 vgarom.asm:764
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03d4 vgarom.asm:765
    out DX, AL                                ; ee                          ; 0xc03d6 vgarom.asm:766
    pop dx                                    ; 5a                          ; 0xc03d7 vgarom.asm:767
    pop ax                                    ; 58                          ; 0xc03d8 vgarom.asm:768
    retn                                      ; c3                          ; 0xc03d9 vgarom.asm:769
    push ax                                   ; 50                          ; 0xc03da vgarom.asm:774
    push dx                                   ; 52                          ; 0xc03db vgarom.asm:775
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03dc vgarom.asm:776
    in AL, DX                                 ; ec                          ; 0xc03df vgarom.asm:777
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03e0 vgarom.asm:778
    pop dx                                    ; 5a                          ; 0xc03e2 vgarom.asm:779
    pop ax                                    ; 58                          ; 0xc03e3 vgarom.asm:780
    retn                                      ; c3                          ; 0xc03e4 vgarom.asm:781
    push ax                                   ; 50                          ; 0xc03e5 vgarom.asm:786
    push dx                                   ; 52                          ; 0xc03e6 vgarom.asm:787
    mov dx, 003dah                            ; ba da 03                    ; 0xc03e7 vgarom.asm:788
    in AL, DX                                 ; ec                          ; 0xc03ea vgarom.asm:789
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03eb vgarom.asm:790
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc03ee vgarom.asm:791
    out DX, AL                                ; ee                          ; 0xc03f0 vgarom.asm:792
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc03f1 vgarom.asm:793
    in AL, DX                                 ; ec                          ; 0xc03f4 vgarom.asm:794
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03f5 vgarom.asm:795
    shr bl, 007h                              ; c0 eb 07                    ; 0xc03f7 vgarom.asm:797
    mov dx, 003dah                            ; ba da 03                    ; 0xc03fa vgarom.asm:807
    in AL, DX                                 ; ec                          ; 0xc03fd vgarom.asm:808
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03fe vgarom.asm:809
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0401 vgarom.asm:810
    out DX, AL                                ; ee                          ; 0xc0403 vgarom.asm:811
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0404 vgarom.asm:812
    in AL, DX                                 ; ec                          ; 0xc0407 vgarom.asm:813
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0408 vgarom.asm:814
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc040a vgarom.asm:815
    test bl, 001h                             ; f6 c3 01                    ; 0xc040d vgarom.asm:816
    jne short 00415h                          ; 75 03                       ; 0xc0410 vgarom.asm:817
    shr bh, 002h                              ; c0 ef 02                    ; 0xc0412 vgarom.asm:819
    mov dx, 003dah                            ; ba da 03                    ; 0xc0415 vgarom.asm:825
    in AL, DX                                 ; ec                          ; 0xc0418 vgarom.asm:826
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0419 vgarom.asm:827
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc041c vgarom.asm:828
    out DX, AL                                ; ee                          ; 0xc041e vgarom.asm:829
    mov dx, 003dah                            ; ba da 03                    ; 0xc041f vgarom.asm:831
    in AL, DX                                 ; ec                          ; 0xc0422 vgarom.asm:832
    pop dx                                    ; 5a                          ; 0xc0423 vgarom.asm:834
    pop ax                                    ; 58                          ; 0xc0424 vgarom.asm:835
    retn                                      ; c3                          ; 0xc0425 vgarom.asm:836
    push ax                                   ; 50                          ; 0xc0426 vgarom.asm:841
    push dx                                   ; 52                          ; 0xc0427 vgarom.asm:842
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0428 vgarom.asm:843
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc042b vgarom.asm:844
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc042d vgarom.asm:845
    out DX, ax                                ; ef                          ; 0xc042f vgarom.asm:846
    pop dx                                    ; 5a                          ; 0xc0430 vgarom.asm:847
    pop ax                                    ; 58                          ; 0xc0431 vgarom.asm:848
    retn                                      ; c3                          ; 0xc0432 vgarom.asm:849
    push DS                                   ; 1e                          ; 0xc0433 vgarom.asm:854
    push ax                                   ; 50                          ; 0xc0434 vgarom.asm:855
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0435 vgarom.asm:856
    mov ds, ax                                ; 8e d8                       ; 0xc0438 vgarom.asm:857
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc043a vgarom.asm:858
    mov bx, 00088h                            ; bb 88 00                    ; 0xc043c vgarom.asm:859
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc043f vgarom.asm:860
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc0441 vgarom.asm:861
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc0444 vgarom.asm:862
    mov ax, word [bx]                         ; 8b 07                       ; 0xc0447 vgarom.asm:863
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0449 vgarom.asm:864
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc044c vgarom.asm:865
    jne short 00453h                          ; 75 02                       ; 0xc044f vgarom.asm:866
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc0451 vgarom.asm:867
    pop ax                                    ; 58                          ; 0xc0453 vgarom.asm:869
    pop DS                                    ; 1f                          ; 0xc0454 vgarom.asm:870
    retn                                      ; c3                          ; 0xc0455 vgarom.asm:871
    push DS                                   ; 1e                          ; 0xc0456 vgarom.asm:879
    push bx                                   ; 53                          ; 0xc0457 vgarom.asm:880
    push dx                                   ; 52                          ; 0xc0458 vgarom.asm:881
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0459 vgarom.asm:882
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc045b vgarom.asm:883
    mov ds, ax                                ; 8e d8                       ; 0xc045e vgarom.asm:884
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0460 vgarom.asm:885
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0463 vgarom.asm:886
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0465 vgarom.asm:887
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0468 vgarom.asm:888
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc046a vgarom.asm:889
    je short 00484h                           ; 74 15                       ; 0xc046d vgarom.asm:890
    jc short 0048eh                           ; 72 1d                       ; 0xc046f vgarom.asm:891
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc0471 vgarom.asm:892
    je short 00478h                           ; 74 02                       ; 0xc0474 vgarom.asm:893
    jmp short 004a2h                          ; eb 2a                       ; 0xc0476 vgarom.asm:903
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc0478 vgarom.asm:909
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc047a vgarom.asm:910
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc047c vgarom.asm:911
    or ah, 009h                               ; 80 cc 09                    ; 0xc047f vgarom.asm:912
    jne short 00498h                          ; 75 14                       ; 0xc0482 vgarom.asm:913
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc0484 vgarom.asm:919
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0486 vgarom.asm:920
    or ah, 009h                               ; 80 cc 09                    ; 0xc0489 vgarom.asm:921
    jne short 00498h                          ; 75 0a                       ; 0xc048c vgarom.asm:922
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc048e vgarom.asm:928
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc0490 vgarom.asm:929
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0492 vgarom.asm:930
    or ah, 008h                               ; 80 cc 08                    ; 0xc0495 vgarom.asm:931
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0498 vgarom.asm:933
    mov byte [bx], al                         ; 88 07                       ; 0xc049b vgarom.asm:934
    mov bx, 00088h                            ; bb 88 00                    ; 0xc049d vgarom.asm:935
    mov byte [bx], ah                         ; 88 27                       ; 0xc04a0 vgarom.asm:936
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04a2 vgarom.asm:938
    pop dx                                    ; 5a                          ; 0xc04a5 vgarom.asm:939
    pop bx                                    ; 5b                          ; 0xc04a6 vgarom.asm:940
    pop DS                                    ; 1f                          ; 0xc04a7 vgarom.asm:941
    retn                                      ; c3                          ; 0xc04a8 vgarom.asm:942
    push DS                                   ; 1e                          ; 0xc04a9 vgarom.asm:951
    push bx                                   ; 53                          ; 0xc04aa vgarom.asm:952
    push dx                                   ; 52                          ; 0xc04ab vgarom.asm:953
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04ac vgarom.asm:954
    and dl, 001h                              ; 80 e2 01                    ; 0xc04ae vgarom.asm:955
    sal dl, 003h                              ; c0 e2 03                    ; 0xc04b1 vgarom.asm:957
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04b4 vgarom.asm:963
    mov ds, ax                                ; 8e d8                       ; 0xc04b7 vgarom.asm:964
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04b9 vgarom.asm:965
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04bc vgarom.asm:966
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04be vgarom.asm:967
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04c0 vgarom.asm:968
    mov byte [bx], al                         ; 88 07                       ; 0xc04c2 vgarom.asm:969
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04c4 vgarom.asm:970
    pop dx                                    ; 5a                          ; 0xc04c7 vgarom.asm:971
    pop bx                                    ; 5b                          ; 0xc04c8 vgarom.asm:972
    pop DS                                    ; 1f                          ; 0xc04c9 vgarom.asm:973
    retn                                      ; c3                          ; 0xc04ca vgarom.asm:974
    push bx                                   ; 53                          ; 0xc04cb vgarom.asm:978
    push dx                                   ; 52                          ; 0xc04cc vgarom.asm:979
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04cd vgarom.asm:980
    and bl, 001h                              ; 80 e3 01                    ; 0xc04cf vgarom.asm:981
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04d2 vgarom.asm:982
    sal bl, 1                                 ; d0 e3                       ; 0xc04d5 vgarom.asm:983
    mov dx, 003cch                            ; ba cc 03                    ; 0xc04d7 vgarom.asm:984
    in AL, DX                                 ; ec                          ; 0xc04da vgarom.asm:985
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc04db vgarom.asm:986
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc04dd vgarom.asm:987
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc04df vgarom.asm:988
    out DX, AL                                ; ee                          ; 0xc04e2 vgarom.asm:989
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04e3 vgarom.asm:990
    pop dx                                    ; 5a                          ; 0xc04e6 vgarom.asm:991
    pop bx                                    ; 5b                          ; 0xc04e7 vgarom.asm:992
    retn                                      ; c3                          ; 0xc04e8 vgarom.asm:993
    push DS                                   ; 1e                          ; 0xc04e9 vgarom.asm:997
    push bx                                   ; 53                          ; 0xc04ea vgarom.asm:998
    push dx                                   ; 52                          ; 0xc04eb vgarom.asm:999
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04ec vgarom.asm:1000
    and dl, 001h                              ; 80 e2 01                    ; 0xc04ee vgarom.asm:1001
    xor dl, 001h                              ; 80 f2 01                    ; 0xc04f1 vgarom.asm:1002
    sal dl, 1                                 ; d0 e2                       ; 0xc04f4 vgarom.asm:1003
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04f6 vgarom.asm:1004
    mov ds, ax                                ; 8e d8                       ; 0xc04f9 vgarom.asm:1005
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04fb vgarom.asm:1006
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04fe vgarom.asm:1007
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0500 vgarom.asm:1008
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0502 vgarom.asm:1009
    mov byte [bx], al                         ; 88 07                       ; 0xc0504 vgarom.asm:1010
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0506 vgarom.asm:1011
    pop dx                                    ; 5a                          ; 0xc0509 vgarom.asm:1012
    pop bx                                    ; 5b                          ; 0xc050a vgarom.asm:1013
    pop DS                                    ; 1f                          ; 0xc050b vgarom.asm:1014
    retn                                      ; c3                          ; 0xc050c vgarom.asm:1015
    push DS                                   ; 1e                          ; 0xc050d vgarom.asm:1019
    push bx                                   ; 53                          ; 0xc050e vgarom.asm:1020
    push dx                                   ; 52                          ; 0xc050f vgarom.asm:1021
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0510 vgarom.asm:1022
    and dl, 001h                              ; 80 e2 01                    ; 0xc0512 vgarom.asm:1023
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0515 vgarom.asm:1024
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0518 vgarom.asm:1025
    mov ds, ax                                ; 8e d8                       ; 0xc051b vgarom.asm:1026
    mov bx, 00089h                            ; bb 89 00                    ; 0xc051d vgarom.asm:1027
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0520 vgarom.asm:1028
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0522 vgarom.asm:1029
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0524 vgarom.asm:1030
    mov byte [bx], al                         ; 88 07                       ; 0xc0526 vgarom.asm:1031
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0528 vgarom.asm:1032
    pop dx                                    ; 5a                          ; 0xc052b vgarom.asm:1033
    pop bx                                    ; 5b                          ; 0xc052c vgarom.asm:1034
    pop DS                                    ; 1f                          ; 0xc052d vgarom.asm:1035
    retn                                      ; c3                          ; 0xc052e vgarom.asm:1036
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc052f vgarom.asm:1041
    je short 00538h                           ; 74 05                       ; 0xc0531 vgarom.asm:1042
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0533 vgarom.asm:1043
    je short 0054dh                           ; 74 16                       ; 0xc0535 vgarom.asm:1044
    retn                                      ; c3                          ; 0xc0537 vgarom.asm:1048
    push DS                                   ; 1e                          ; 0xc0538 vgarom.asm:1050
    push ax                                   ; 50                          ; 0xc0539 vgarom.asm:1051
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc053a vgarom.asm:1052
    mov ds, ax                                ; 8e d8                       ; 0xc053d vgarom.asm:1053
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc053f vgarom.asm:1054
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0542 vgarom.asm:1055
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0544 vgarom.asm:1056
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0546 vgarom.asm:1057
    pop ax                                    ; 58                          ; 0xc0548 vgarom.asm:1058
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0549 vgarom.asm:1059
    pop DS                                    ; 1f                          ; 0xc054b vgarom.asm:1060
    retn                                      ; c3                          ; 0xc054c vgarom.asm:1061
    push DS                                   ; 1e                          ; 0xc054d vgarom.asm:1063
    push ax                                   ; 50                          ; 0xc054e vgarom.asm:1064
    push bx                                   ; 53                          ; 0xc054f vgarom.asm:1065
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0550 vgarom.asm:1066
    mov ds, ax                                ; 8e d8                       ; 0xc0553 vgarom.asm:1067
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0555 vgarom.asm:1068
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0557 vgarom.asm:1069
    mov byte [bx], al                         ; 88 07                       ; 0xc055a vgarom.asm:1070
    pop bx                                    ; 5b                          ; 0xc055c vgarom.asm:1080
    pop ax                                    ; 58                          ; 0xc055d vgarom.asm:1081
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc055e vgarom.asm:1082
    pop DS                                    ; 1f                          ; 0xc0560 vgarom.asm:1083
    retn                                      ; c3                          ; 0xc0561 vgarom.asm:1084
    times 0xe db 0
  ; disGetNextSymbol 0xc0570 LB 0x38d -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x386 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
do_in_ax_dx:                                 ; 0xc0577 LB 0x40
    in AL, DX                                 ; ec                          ; 0xc0577 vberom.asm:76
    xchg ah, al                               ; 86 c4                       ; 0xc0578 vberom.asm:77
    in AL, DX                                 ; ec                          ; 0xc057a vberom.asm:78
    retn                                      ; c3                          ; 0xc057b vberom.asm:79
    push ax                                   ; 50                          ; 0xc057c vberom.asm:90
    push dx                                   ; 52                          ; 0xc057d vberom.asm:91
    mov dx, 003dah                            ; ba da 03                    ; 0xc057e vberom.asm:92
    in AL, DX                                 ; ec                          ; 0xc0581 vberom.asm:94
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0582 vberom.asm:95
    je short 00581h                           ; 74 fb                       ; 0xc0584 vberom.asm:96
    pop dx                                    ; 5a                          ; 0xc0586 vberom.asm:97
    pop ax                                    ; 58                          ; 0xc0587 vberom.asm:98
    retn                                      ; c3                          ; 0xc0588 vberom.asm:99
    push ax                                   ; 50                          ; 0xc0589 vberom.asm:102
    push dx                                   ; 52                          ; 0xc058a vberom.asm:103
    mov dx, 003dah                            ; ba da 03                    ; 0xc058b vberom.asm:104
    in AL, DX                                 ; ec                          ; 0xc058e vberom.asm:106
    test AL, strict byte 008h                 ; a8 08                       ; 0xc058f vberom.asm:107
    jne short 0058eh                          ; 75 fb                       ; 0xc0591 vberom.asm:108
    pop dx                                    ; 5a                          ; 0xc0593 vberom.asm:109
    pop ax                                    ; 58                          ; 0xc0594 vberom.asm:110
    retn                                      ; c3                          ; 0xc0595 vberom.asm:111
    push dx                                   ; 52                          ; 0xc0596 vberom.asm:116
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0597 vberom.asm:117
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc059a vberom.asm:118
    call 00570h                               ; e8 d0 ff                    ; 0xc059d vberom.asm:119
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05a0 vberom.asm:120
    call 00577h                               ; e8 d1 ff                    ; 0xc05a3 vberom.asm:121
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc05a6 vberom.asm:122
    jbe short 005b5h                          ; 76 0b                       ; 0xc05a8 vberom.asm:123
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc05aa vberom.asm:124
    shr ah, 003h                              ; c0 ec 03                    ; 0xc05ac vberom.asm:126
    test AL, strict byte 007h                 ; a8 07                       ; 0xc05af vberom.asm:132
    je short 005b5h                           ; 74 02                       ; 0xc05b1 vberom.asm:133
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc05b3 vberom.asm:134
    pop dx                                    ; 5a                          ; 0xc05b5 vberom.asm:136
    retn                                      ; c3                          ; 0xc05b6 vberom.asm:137
  ; disGetNextSymbol 0xc05b7 LB 0x346 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
_dispi_get_max_bpp:                          ; 0xc05b7 LB 0x26
    push dx                                   ; 52                          ; 0xc05b7 vberom.asm:142
    push bx                                   ; 53                          ; 0xc05b8 vberom.asm:143
    call 005f1h                               ; e8 35 00                    ; 0xc05b9 vberom.asm:144
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc05bc vberom.asm:145
    or ax, strict byte 00002h                 ; 83 c8 02                    ; 0xc05be vberom.asm:146
    call 005ddh                               ; e8 19 00                    ; 0xc05c1 vberom.asm:147
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05c4 vberom.asm:148
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05c7 vberom.asm:149
    call 00570h                               ; e8 a3 ff                    ; 0xc05ca vberom.asm:150
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05cd vberom.asm:151
    call 00577h                               ; e8 a4 ff                    ; 0xc05d0 vberom.asm:152
    push ax                                   ; 50                          ; 0xc05d3 vberom.asm:153
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc05d4 vberom.asm:154
    call 005ddh                               ; e8 04 00                    ; 0xc05d6 vberom.asm:155
    pop ax                                    ; 58                          ; 0xc05d9 vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc05da vberom.asm:157
    pop dx                                    ; 5a                          ; 0xc05db vberom.asm:158
    retn                                      ; c3                          ; 0xc05dc vberom.asm:159
  ; disGetNextSymbol 0xc05dd LB 0x320 -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
dispi_set_enable_:                           ; 0xc05dd LB 0x26
    push dx                                   ; 52                          ; 0xc05dd vberom.asm:162
    push ax                                   ; 50                          ; 0xc05de vberom.asm:163
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05df vberom.asm:164
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05e2 vberom.asm:165
    call 00570h                               ; e8 88 ff                    ; 0xc05e5 vberom.asm:166
    pop ax                                    ; 58                          ; 0xc05e8 vberom.asm:167
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05e9 vberom.asm:168
    call 00570h                               ; e8 81 ff                    ; 0xc05ec vberom.asm:169
    pop dx                                    ; 5a                          ; 0xc05ef vberom.asm:170
    retn                                      ; c3                          ; 0xc05f0 vberom.asm:171
    push dx                                   ; 52                          ; 0xc05f1 vberom.asm:174
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05f2 vberom.asm:175
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05f5 vberom.asm:176
    call 00570h                               ; e8 75 ff                    ; 0xc05f8 vberom.asm:177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05fb vberom.asm:178
    call 00577h                               ; e8 76 ff                    ; 0xc05fe vberom.asm:179
    pop dx                                    ; 5a                          ; 0xc0601 vberom.asm:180
    retn                                      ; c3                          ; 0xc0602 vberom.asm:181
  ; disGetNextSymbol 0xc0603 LB 0x2fa -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
dispi_set_bank_:                             ; 0xc0603 LB 0x26
    push dx                                   ; 52                          ; 0xc0603 vberom.asm:184
    push ax                                   ; 50                          ; 0xc0604 vberom.asm:185
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0605 vberom.asm:186
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0608 vberom.asm:187
    call 00570h                               ; e8 62 ff                    ; 0xc060b vberom.asm:188
    pop ax                                    ; 58                          ; 0xc060e vberom.asm:189
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc060f vberom.asm:190
    call 00570h                               ; e8 5b ff                    ; 0xc0612 vberom.asm:191
    pop dx                                    ; 5a                          ; 0xc0615 vberom.asm:192
    retn                                      ; c3                          ; 0xc0616 vberom.asm:193
    push dx                                   ; 52                          ; 0xc0617 vberom.asm:196
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0618 vberom.asm:197
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc061b vberom.asm:198
    call 00570h                               ; e8 4f ff                    ; 0xc061e vberom.asm:199
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0621 vberom.asm:200
    call 00577h                               ; e8 50 ff                    ; 0xc0624 vberom.asm:201
    pop dx                                    ; 5a                          ; 0xc0627 vberom.asm:202
    retn                                      ; c3                          ; 0xc0628 vberom.asm:203
  ; disGetNextSymbol 0xc0629 LB 0x2d4 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
_dispi_set_bank_farcall:                     ; 0xc0629 LB 0xa9
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc0629 vberom.asm:206
    je short 00653h                           ; 74 24                       ; 0xc062d vberom.asm:207
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db                     ; 0xc062f vberom.asm:208
    jne short 00665h                          ; 75 32                       ; 0xc0631 vberom.asm:209
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0633 vberom.asm:210
    push dx                                   ; 52                          ; 0xc0635 vberom.asm:211
    push ax                                   ; 50                          ; 0xc0636 vberom.asm:212
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0637 vberom.asm:213
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc063a vberom.asm:214
    call 00570h                               ; e8 30 ff                    ; 0xc063d vberom.asm:215
    pop ax                                    ; 58                          ; 0xc0640 vberom.asm:216
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0641 vberom.asm:217
    call 00570h                               ; e8 29 ff                    ; 0xc0644 vberom.asm:218
    call 00577h                               ; e8 2d ff                    ; 0xc0647 vberom.asm:219
    pop dx                                    ; 5a                          ; 0xc064a vberom.asm:220
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0                     ; 0xc064b vberom.asm:221
    jne short 00665h                          ; 75 16                       ; 0xc064d vberom.asm:222
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc064f vberom.asm:223
    retf                                      ; cb                          ; 0xc0652 vberom.asm:224
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0653 vberom.asm:226
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0656 vberom.asm:227
    call 00570h                               ; e8 14 ff                    ; 0xc0659 vberom.asm:228
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc065c vberom.asm:229
    call 00577h                               ; e8 15 ff                    ; 0xc065f vberom.asm:230
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0662 vberom.asm:231
    retf                                      ; cb                          ; 0xc0664 vberom.asm:232
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0665 vberom.asm:234
    retf                                      ; cb                          ; 0xc0668 vberom.asm:235
    push dx                                   ; 52                          ; 0xc0669 vberom.asm:238
    push ax                                   ; 50                          ; 0xc066a vberom.asm:239
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc066b vberom.asm:240
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc066e vberom.asm:241
    call 00570h                               ; e8 fc fe                    ; 0xc0671 vberom.asm:242
    pop ax                                    ; 58                          ; 0xc0674 vberom.asm:243
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0675 vberom.asm:244
    call 00570h                               ; e8 f5 fe                    ; 0xc0678 vberom.asm:245
    pop dx                                    ; 5a                          ; 0xc067b vberom.asm:246
    retn                                      ; c3                          ; 0xc067c vberom.asm:247
    push dx                                   ; 52                          ; 0xc067d vberom.asm:250
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc067e vberom.asm:251
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0681 vberom.asm:252
    call 00570h                               ; e8 e9 fe                    ; 0xc0684 vberom.asm:253
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0687 vberom.asm:254
    call 00577h                               ; e8 ea fe                    ; 0xc068a vberom.asm:255
    pop dx                                    ; 5a                          ; 0xc068d vberom.asm:256
    retn                                      ; c3                          ; 0xc068e vberom.asm:257
    push dx                                   ; 52                          ; 0xc068f vberom.asm:260
    push ax                                   ; 50                          ; 0xc0690 vberom.asm:261
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0691 vberom.asm:262
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0694 vberom.asm:263
    call 00570h                               ; e8 d6 fe                    ; 0xc0697 vberom.asm:264
    pop ax                                    ; 58                          ; 0xc069a vberom.asm:265
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc069b vberom.asm:266
    call 00570h                               ; e8 cf fe                    ; 0xc069e vberom.asm:267
    pop dx                                    ; 5a                          ; 0xc06a1 vberom.asm:268
    retn                                      ; c3                          ; 0xc06a2 vberom.asm:269
    push dx                                   ; 52                          ; 0xc06a3 vberom.asm:272
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06a4 vberom.asm:273
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06a7 vberom.asm:274
    call 00570h                               ; e8 c3 fe                    ; 0xc06aa vberom.asm:275
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06ad vberom.asm:276
    call 00577h                               ; e8 c4 fe                    ; 0xc06b0 vberom.asm:277
    pop dx                                    ; 5a                          ; 0xc06b3 vberom.asm:278
    retn                                      ; c3                          ; 0xc06b4 vberom.asm:279
    push ax                                   ; 50                          ; 0xc06b5 vberom.asm:282
    push bx                                   ; 53                          ; 0xc06b6 vberom.asm:283
    push dx                                   ; 52                          ; 0xc06b7 vberom.asm:284
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc06b8 vberom.asm:285
    call 00596h                               ; e8 d9 fe                    ; 0xc06ba vberom.asm:286
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc06bd vberom.asm:287
    jnbe short 006c3h                         ; 77 02                       ; 0xc06bf vberom.asm:288
    shr bx, 1                                 ; d1 eb                       ; 0xc06c1 vberom.asm:289
    shr bx, 003h                              ; c1 eb 03                    ; 0xc06c3 vberom.asm:292
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06c6 vberom.asm:298
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc06c9 vberom.asm:299
    mov AL, strict byte 013h                  ; b0 13                       ; 0xc06cb vberom.asm:300
    out DX, ax                                ; ef                          ; 0xc06cd vberom.asm:301
    pop dx                                    ; 5a                          ; 0xc06ce vberom.asm:302
    pop bx                                    ; 5b                          ; 0xc06cf vberom.asm:303
    pop ax                                    ; 58                          ; 0xc06d0 vberom.asm:304
    retn                                      ; c3                          ; 0xc06d1 vberom.asm:305
  ; disGetNextSymbol 0xc06d2 LB 0x22b -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
_vga_compat_setup:                           ; 0xc06d2 LB 0xed
    push ax                                   ; 50                          ; 0xc06d2 vberom.asm:308
    push dx                                   ; 52                          ; 0xc06d3 vberom.asm:309
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06d4 vberom.asm:312
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc06d7 vberom.asm:313
    call 00570h                               ; e8 93 fe                    ; 0xc06da vberom.asm:314
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06dd vberom.asm:315
    call 00577h                               ; e8 94 fe                    ; 0xc06e0 vberom.asm:316
    push ax                                   ; 50                          ; 0xc06e3 vberom.asm:317
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06e4 vberom.asm:318
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc06e7 vberom.asm:319
    out DX, ax                                ; ef                          ; 0xc06ea vberom.asm:320
    pop ax                                    ; 58                          ; 0xc06eb vberom.asm:321
    push ax                                   ; 50                          ; 0xc06ec vberom.asm:322
    shr ax, 003h                              ; c1 e8 03                    ; 0xc06ed vberom.asm:324
    dec ax                                    ; 48                          ; 0xc06f0 vberom.asm:330
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc06f1 vberom.asm:331
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc06f3 vberom.asm:332
    out DX, ax                                ; ef                          ; 0xc06f5 vberom.asm:333
    pop ax                                    ; 58                          ; 0xc06f6 vberom.asm:334
    call 006b5h                               ; e8 bb ff                    ; 0xc06f7 vberom.asm:335
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06fa vberom.asm:338
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc06fd vberom.asm:339
    call 00570h                               ; e8 6d fe                    ; 0xc0700 vberom.asm:340
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0703 vberom.asm:341
    call 00577h                               ; e8 6e fe                    ; 0xc0706 vberom.asm:342
    dec ax                                    ; 48                          ; 0xc0709 vberom.asm:343
    push ax                                   ; 50                          ; 0xc070a vberom.asm:344
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc070b vberom.asm:345
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc070e vberom.asm:346
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc0710 vberom.asm:347
    out DX, ax                                ; ef                          ; 0xc0712 vberom.asm:348
    pop ax                                    ; 58                          ; 0xc0713 vberom.asm:349
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc0714 vberom.asm:350
    out DX, AL                                ; ee                          ; 0xc0716 vberom.asm:351
    inc dx                                    ; 42                          ; 0xc0717 vberom.asm:352
    in AL, DX                                 ; ec                          ; 0xc0718 vberom.asm:353
    and AL, strict byte 0bdh                  ; 24 bd                       ; 0xc0719 vberom.asm:354
    test ah, 001h                             ; f6 c4 01                    ; 0xc071b vberom.asm:355
    je short 00722h                           ; 74 02                       ; 0xc071e vberom.asm:356
    or AL, strict byte 002h                   ; 0c 02                       ; 0xc0720 vberom.asm:357
    test ah, 002h                             ; f6 c4 02                    ; 0xc0722 vberom.asm:359
    je short 00729h                           ; 74 02                       ; 0xc0725 vberom.asm:360
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0727 vberom.asm:361
    out DX, AL                                ; ee                          ; 0xc0729 vberom.asm:363
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc072a vberom.asm:366
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc072d vberom.asm:367
    out DX, AL                                ; ee                          ; 0xc0730 vberom.asm:368
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0731 vberom.asm:369
    in AL, DX                                 ; ec                          ; 0xc0734 vberom.asm:370
    and AL, strict byte 060h                  ; 24 60                       ; 0xc0735 vberom.asm:371
    out DX, AL                                ; ee                          ; 0xc0737 vberom.asm:372
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0738 vberom.asm:373
    mov AL, strict byte 017h                  ; b0 17                       ; 0xc073b vberom.asm:374
    out DX, AL                                ; ee                          ; 0xc073d vberom.asm:375
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc073e vberom.asm:376
    in AL, DX                                 ; ec                          ; 0xc0741 vberom.asm:377
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc0742 vberom.asm:378
    out DX, AL                                ; ee                          ; 0xc0744 vberom.asm:379
    mov dx, 003dah                            ; ba da 03                    ; 0xc0745 vberom.asm:380
    in AL, DX                                 ; ec                          ; 0xc0748 vberom.asm:381
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0749 vberom.asm:382
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc074c vberom.asm:383
    out DX, AL                                ; ee                          ; 0xc074e vberom.asm:384
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc074f vberom.asm:385
    in AL, DX                                 ; ec                          ; 0xc0752 vberom.asm:386
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc0753 vberom.asm:387
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0755 vberom.asm:388
    out DX, AL                                ; ee                          ; 0xc0758 vberom.asm:389
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0759 vberom.asm:390
    out DX, AL                                ; ee                          ; 0xc075b vberom.asm:391
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc075c vberom.asm:392
    mov ax, 00506h                            ; b8 06 05                    ; 0xc075f vberom.asm:393
    out DX, ax                                ; ef                          ; 0xc0762 vberom.asm:394
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0763 vberom.asm:395
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc0766 vberom.asm:396
    out DX, ax                                ; ef                          ; 0xc0769 vberom.asm:397
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc076a vberom.asm:400
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc076d vberom.asm:401
    call 00570h                               ; e8 fd fd                    ; 0xc0770 vberom.asm:402
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0773 vberom.asm:403
    call 00577h                               ; e8 fe fd                    ; 0xc0776 vberom.asm:404
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc0779 vberom.asm:405
    jc short 007bdh                           ; 72 40                       ; 0xc077b vberom.asm:406
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc077d vberom.asm:407
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0780 vberom.asm:408
    out DX, AL                                ; ee                          ; 0xc0782 vberom.asm:409
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0783 vberom.asm:410
    in AL, DX                                 ; ec                          ; 0xc0786 vberom.asm:411
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0787 vberom.asm:412
    out DX, AL                                ; ee                          ; 0xc0789 vberom.asm:413
    mov dx, 003dah                            ; ba da 03                    ; 0xc078a vberom.asm:414
    in AL, DX                                 ; ec                          ; 0xc078d vberom.asm:415
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc078e vberom.asm:416
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0791 vberom.asm:417
    out DX, AL                                ; ee                          ; 0xc0793 vberom.asm:418
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0794 vberom.asm:419
    in AL, DX                                 ; ec                          ; 0xc0797 vberom.asm:420
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0798 vberom.asm:421
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc079a vberom.asm:422
    out DX, AL                                ; ee                          ; 0xc079d vberom.asm:423
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc079e vberom.asm:424
    out DX, AL                                ; ee                          ; 0xc07a0 vberom.asm:425
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc07a1 vberom.asm:426
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc07a4 vberom.asm:427
    out DX, AL                                ; ee                          ; 0xc07a6 vberom.asm:428
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc07a7 vberom.asm:429
    in AL, DX                                 ; ec                          ; 0xc07aa vberom.asm:430
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc07ab vberom.asm:431
    out DX, AL                                ; ee                          ; 0xc07ad vberom.asm:432
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc07ae vberom.asm:433
    mov AL, strict byte 005h                  ; b0 05                       ; 0xc07b1 vberom.asm:434
    out DX, AL                                ; ee                          ; 0xc07b3 vberom.asm:435
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc07b4 vberom.asm:436
    in AL, DX                                 ; ec                          ; 0xc07b7 vberom.asm:437
    and AL, strict byte 09fh                  ; 24 9f                       ; 0xc07b8 vberom.asm:438
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07ba vberom.asm:439
    out DX, AL                                ; ee                          ; 0xc07bc vberom.asm:440
    pop dx                                    ; 5a                          ; 0xc07bd vberom.asm:443
    pop ax                                    ; 58                          ; 0xc07be vberom.asm:444
  ; disGetNextSymbol 0xc07bf LB 0x13e -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
_vbe_has_vbe_display:                        ; 0xc07bf LB 0x13
    push DS                                   ; 1e                          ; 0xc07bf vberom.asm:450
    push bx                                   ; 53                          ; 0xc07c0 vberom.asm:451
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07c1 vberom.asm:452
    mov ds, ax                                ; 8e d8                       ; 0xc07c4 vberom.asm:453
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc07c6 vberom.asm:454
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07c9 vberom.asm:455
    and AL, strict byte 001h                  ; 24 01                       ; 0xc07cb vberom.asm:456
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc07cd vberom.asm:457
    pop bx                                    ; 5b                          ; 0xc07cf vberom.asm:458
    pop DS                                    ; 1f                          ; 0xc07d0 vberom.asm:459
    retn                                      ; c3                          ; 0xc07d1 vberom.asm:460
  ; disGetNextSymbol 0xc07d2 LB 0x12b -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
vbe_biosfn_return_current_mode:              ; 0xc07d2 LB 0x25
    push DS                                   ; 1e                          ; 0xc07d2 vberom.asm:473
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07d3 vberom.asm:474
    mov ds, ax                                ; 8e d8                       ; 0xc07d6 vberom.asm:475
    call 005f1h                               ; e8 16 fe                    ; 0xc07d8 vberom.asm:476
    and ax, strict byte 00001h                ; 83 e0 01                    ; 0xc07db vberom.asm:477
    je short 007e9h                           ; 74 09                       ; 0xc07de vberom.asm:478
    mov bx, 000bah                            ; bb ba 00                    ; 0xc07e0 vberom.asm:479
    mov ax, word [bx]                         ; 8b 07                       ; 0xc07e3 vberom.asm:480
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc07e5 vberom.asm:481
    jne short 007f2h                          ; 75 09                       ; 0xc07e7 vberom.asm:482
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc07e9 vberom.asm:484
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07ec vberom.asm:485
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc07ee vberom.asm:486
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc07f0 vberom.asm:487
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc07f2 vberom.asm:489
    pop DS                                    ; 1f                          ; 0xc07f5 vberom.asm:490
    retn                                      ; c3                          ; 0xc07f6 vberom.asm:491
  ; disGetNextSymbol 0xc07f7 LB 0x106 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
vbe_biosfn_display_window_control:           ; 0xc07f7 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc07f7 vberom.asm:515
    jne short 00820h                          ; 75 24                       ; 0xc07fa vberom.asm:516
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc07fc vberom.asm:517
    je short 00817h                           ; 74 16                       ; 0xc07ff vberom.asm:518
    jc short 00807h                           ; 72 04                       ; 0xc0801 vberom.asm:519
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0803 vberom.asm:520
    retn                                      ; c3                          ; 0xc0806 vberom.asm:521
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0807 vberom.asm:523
    call 00603h                               ; e8 f7 fd                    ; 0xc0809 vberom.asm:524
    call 00617h                               ; e8 08 fe                    ; 0xc080c vberom.asm:525
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2                     ; 0xc080f vberom.asm:526
    jne short 00820h                          ; 75 0d                       ; 0xc0811 vberom.asm:527
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0813 vberom.asm:528
    retn                                      ; c3                          ; 0xc0816 vberom.asm:529
    call 00617h                               ; e8 fd fd                    ; 0xc0817 vberom.asm:531
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc081a vberom.asm:532
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc081c vberom.asm:533
    retn                                      ; c3                          ; 0xc081f vberom.asm:534
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0820 vberom.asm:536
    retn                                      ; c3                          ; 0xc0823 vberom.asm:537
  ; disGetNextSymbol 0xc0824 LB 0xd9 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
vbe_biosfn_set_get_display_start:            ; 0xc0824 LB 0x34
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc0824 vberom.asm:577
    je short 00834h                           ; 74 0b                       ; 0xc0827 vberom.asm:578
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0829 vberom.asm:579
    je short 00848h                           ; 74 1a                       ; 0xc082c vberom.asm:580
    jc short 0083ah                           ; 72 0a                       ; 0xc082e vberom.asm:581
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0830 vberom.asm:582
    retn                                      ; c3                          ; 0xc0833 vberom.asm:583
    call 00589h                               ; e8 52 fd                    ; 0xc0834 vberom.asm:585
    call 0057ch                               ; e8 42 fd                    ; 0xc0837 vberom.asm:586
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc083a vberom.asm:588
    call 00669h                               ; e8 2a fe                    ; 0xc083c vberom.asm:589
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc083f vberom.asm:590
    call 0068fh                               ; e8 4b fe                    ; 0xc0841 vberom.asm:591
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0844 vberom.asm:592
    retn                                      ; c3                          ; 0xc0847 vberom.asm:593
    call 0067dh                               ; e8 32 fe                    ; 0xc0848 vberom.asm:595
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8                     ; 0xc084b vberom.asm:596
    call 006a3h                               ; e8 53 fe                    ; 0xc084d vberom.asm:597
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0850 vberom.asm:598
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0852 vberom.asm:599
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0854 vberom.asm:600
    retn                                      ; c3                          ; 0xc0857 vberom.asm:601
  ; disGetNextSymbol 0xc0858 LB 0xa5 -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
vbe_biosfn_set_get_dac_palette_format:       ; 0xc0858 LB 0x37
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0858 vberom.asm:616
    je short 0087bh                           ; 74 1e                       ; 0xc085b vberom.asm:617
    jc short 00863h                           ; 72 04                       ; 0xc085d vberom.asm:618
    mov ax, 00100h                            ; b8 00 01                    ; 0xc085f vberom.asm:619
    retn                                      ; c3                          ; 0xc0862 vberom.asm:620
    call 005f1h                               ; e8 8b fd                    ; 0xc0863 vberom.asm:622
    cmp bh, 006h                              ; 80 ff 06                    ; 0xc0866 vberom.asm:623
    je short 00875h                           ; 74 0a                       ; 0xc0869 vberom.asm:624
    cmp bh, 008h                              ; 80 ff 08                    ; 0xc086b vberom.asm:625
    jne short 0088bh                          ; 75 1b                       ; 0xc086e vberom.asm:626
    or ax, strict byte 00020h                 ; 83 c8 20                    ; 0xc0870 vberom.asm:627
    jne short 00878h                          ; 75 03                       ; 0xc0873 vberom.asm:628
    and ax, strict byte 0ffdfh                ; 83 e0 df                    ; 0xc0875 vberom.asm:630
    call 005ddh                               ; e8 62 fd                    ; 0xc0878 vberom.asm:632
    mov BH, strict byte 006h                  ; b7 06                       ; 0xc087b vberom.asm:634
    call 005f1h                               ; e8 71 fd                    ; 0xc087d vberom.asm:635
    and ax, strict byte 00020h                ; 83 e0 20                    ; 0xc0880 vberom.asm:636
    je short 00887h                           ; 74 02                       ; 0xc0883 vberom.asm:637
    mov BH, strict byte 008h                  ; b7 08                       ; 0xc0885 vberom.asm:638
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0887 vberom.asm:640
    retn                                      ; c3                          ; 0xc088a vberom.asm:641
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc088b vberom.asm:643
    retn                                      ; c3                          ; 0xc088e vberom.asm:644
  ; disGetNextSymbol 0xc088f LB 0x6e -> off=0x0 cb=0000000000000057 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x57
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008c2h                           ; 74 2a                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008e2h                          ; 76 45                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008deh                          ; 75 3c                       ; 0xc08a0 vberom.asm:690
    pushaw                                    ; 60                          ; 0xc08a2 vberom.asm:133
    push DS                                   ; 1e                          ; 0xc08a3 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08a4 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08a5 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08a6 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08a8 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08ab vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08ac vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08ad vberom.asm:703
    lodsw                                     ; ad                          ; 0xc08af vberom.asm:714
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc08b0 vberom.asm:715
    lodsw                                     ; ad                          ; 0xc08b2 vberom.asm:716
    out DX, AL                                ; ee                          ; 0xc08b3 vberom.asm:717
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc08b4 vberom.asm:718
    out DX, AL                                ; ee                          ; 0xc08b6 vberom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc08b7 vberom.asm:720
    out DX, AL                                ; ee                          ; 0xc08b9 vberom.asm:721
    loop 008afh                               ; e2 f3                       ; 0xc08ba vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08bc vberom.asm:724
    popaw                                     ; 61                          ; 0xc08bd vberom.asm:152
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08be vberom.asm:727
    retn                                      ; c3                          ; 0xc08c1 vberom.asm:728
    pushaw                                    ; 60                          ; 0xc08c2 vberom.asm:133
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08c3 vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08c5 vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08c8 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08c9 vberom.asm:735
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db                     ; 0xc08cc vberom.asm:746
    in AL, DX                                 ; ec                          ; 0xc08ce vberom.asm:748
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc08cf vberom.asm:749
    in AL, DX                                 ; ec                          ; 0xc08d1 vberom.asm:750
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc08d2 vberom.asm:751
    in AL, DX                                 ; ec                          ; 0xc08d4 vberom.asm:752
    stosw                                     ; ab                          ; 0xc08d5 vberom.asm:753
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc08d6 vberom.asm:754
    stosw                                     ; ab                          ; 0xc08d8 vberom.asm:755
    loop 008ceh                               ; e2 f3                       ; 0xc08d9 vberom.asm:757
    popaw                                     ; 61                          ; 0xc08db vberom.asm:152
    jmp short 008beh                          ; eb e0                       ; 0xc08dc vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08de vberom.asm:762
    retn                                      ; c3                          ; 0xc08e1 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08e2 vberom.asm:765
    retn                                      ; c3                          ; 0xc08e5 vberom.asm:766
  ; disGetNextSymbol 0xc08e6 LB 0x17 -> off=0x0 cb=0000000000000017 uValue=00000000000c08e6 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08e6 LB 0x17
    test bl, bl                               ; 84 db                       ; 0xc08e6 vberom.asm:780
    jne short 008f9h                          ; 75 0f                       ; 0xc08e8 vberom.asm:781
    mov di, 0c000h                            ; bf 00 c0                    ; 0xc08ea vberom.asm:782
    mov es, di                                ; 8e c7                       ; 0xc08ed vberom.asm:783
    mov di, 04400h                            ; bf 00 44                    ; 0xc08ef vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08f2 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08f5 vberom.asm:786
    retn                                      ; c3                          ; 0xc08f8 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08f9 vberom.asm:789
    retn                                      ; c3                          ; 0xc08fc vberom.asm:790

  ; Padding 0x103 bytes at 0xc08fd
  times 259 db 0

section _TEXT progbits vstart=0xa00 align=1 ; size=0x32ed class=CODE group=AUTO
  ; disGetNextSymbol 0xc0a00 LB 0x32ed -> off=0x0 cb=000000000000001b uValue=00000000000c0a00 'set_int_vector'
set_int_vector:                              ; 0xc0a00 LB 0x1b
    push bx                                   ; 53                          ; 0xc0a00 vgabios.c:85
    push bp                                   ; 55                          ; 0xc0a01
    mov bp, sp                                ; 89 e5                       ; 0xc0a02
    mov bl, al                                ; 88 c3                       ; 0xc0a04
    xor bh, bh                                ; 30 ff                       ; 0xc0a06 vgabios.c:89
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0a08
    xor ax, ax                                ; 31 c0                       ; 0xc0a0b
    mov es, ax                                ; 8e c0                       ; 0xc0a0d
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a0f
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0a12
    pop bp                                    ; 5d                          ; 0xc0a18 vgabios.c:90
    pop bx                                    ; 5b                          ; 0xc0a19
    retn                                      ; c3                          ; 0xc0a1a
  ; disGetNextSymbol 0xc0a1b LB 0x32d2 -> off=0x0 cb=000000000000001c uValue=00000000000c0a1b 'init_vga_card'
init_vga_card:                               ; 0xc0a1b LB 0x1c
    push bp                                   ; 55                          ; 0xc0a1b vgabios.c:141
    mov bp, sp                                ; 89 e5                       ; 0xc0a1c
    push dx                                   ; 52                          ; 0xc0a1e
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a1f vgabios.c:144
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a21
    out DX, AL                                ; ee                          ; 0xc0a24
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a25 vgabios.c:147
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a27
    out DX, AL                                ; ee                          ; 0xc0a2a
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a2b vgabios.c:148
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a2d
    out DX, AL                                ; ee                          ; 0xc0a30
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a31 vgabios.c:153
    pop dx                                    ; 5a                          ; 0xc0a34
    pop bp                                    ; 5d                          ; 0xc0a35
    retn                                      ; c3                          ; 0xc0a36
  ; disGetNextSymbol 0xc0a37 LB 0x32b6 -> off=0x0 cb=0000000000000032 uValue=00000000000c0a37 'init_bios_area'
init_bios_area:                              ; 0xc0a37 LB 0x32
    push bx                                   ; 53                          ; 0xc0a37 vgabios.c:162
    push bp                                   ; 55                          ; 0xc0a38
    mov bp, sp                                ; 89 e5                       ; 0xc0a39
    xor bx, bx                                ; 31 db                       ; 0xc0a3b vgabios.c:166
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a3d
    mov es, ax                                ; 8e c0                       ; 0xc0a40
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a42 vgabios.c:169
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a46
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a48
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a4a
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a4e vgabios.c:173
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a54 vgabios.c:175
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a5b vgabios.c:179
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a61 vgabios.c:181
    pop bp                                    ; 5d                          ; 0xc0a66 vgabios.c:182
    pop bx                                    ; 5b                          ; 0xc0a67
    retn                                      ; c3                          ; 0xc0a68
  ; disGetNextSymbol 0xc0a69 LB 0x3284 -> off=0x0 cb=0000000000000022 uValue=00000000000c0a69 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a69 LB 0x22
    inc bp                                    ; 45                          ; 0xc0a69 vgabios.c:222
    push bp                                   ; 55                          ; 0xc0a6a
    mov bp, sp                                ; 89 e5                       ; 0xc0a6b
    call 00a1bh                               ; e8 ab ff                    ; 0xc0a6d vgabios.c:224
    call 00a37h                               ; e8 c4 ff                    ; 0xc0a70 vgabios.c:225
    call 0372ch                               ; e8 b6 2c                    ; 0xc0a73 vgabios.c:227
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc0a76 vgabios.c:229
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a79
    call 00a00h                               ; e8 81 ff                    ; 0xc0a7c
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a7f vgabios.c:255
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a82
    int 010h                                  ; cd 10                       ; 0xc0a84
    mov sp, bp                                ; 89 ec                       ; 0xc0a86 vgabios.c:258
    pop bp                                    ; 5d                          ; 0xc0a88
    dec bp                                    ; 4d                          ; 0xc0a89
    retf                                      ; cb                          ; 0xc0a8a
  ; disGetNextSymbol 0xc0a8b LB 0x3262 -> off=0x0 cb=0000000000000046 uValue=00000000000c0a8b 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a8b LB 0x46
    push bp                                   ; 55                          ; 0xc0a8b vgabios.c:327
    mov bp, sp                                ; 89 e5                       ; 0xc0a8c
    push cx                                   ; 51                          ; 0xc0a8e
    push si                                   ; 56                          ; 0xc0a8f
    mov cl, al                                ; 88 c1                       ; 0xc0a90
    mov si, dx                                ; 89 d6                       ; 0xc0a92
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a94 vgabios.c:329
    jbe short 00aa6h                          ; 76 0e                       ; 0xc0a96
    push SS                                   ; 16                          ; 0xc0a98 vgabios.c:330
    pop ES                                    ; 07                          ; 0xc0a99
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a9a
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a9f vgabios.c:331
    jmp short 00acah                          ; eb 24                       ; 0xc0aa4 vgabios.c:332
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0aa6 vgabios.c:334
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0aa9
    call 031a4h                               ; e8 f5 26                    ; 0xc0aac
    push SS                                   ; 16                          ; 0xc0aaf
    pop ES                                    ; 07                          ; 0xc0ab0
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0ab1
    mov al, cl                                ; 88 c8                       ; 0xc0ab4 vgabios.c:335
    xor ah, ah                                ; 30 e4                       ; 0xc0ab6
    mov dx, ax                                ; 89 c2                       ; 0xc0ab8
    add dx, ax                                ; 01 c2                       ; 0xc0aba
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0abc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0abf
    call 031a4h                               ; e8 df 26                    ; 0xc0ac2
    push SS                                   ; 16                          ; 0xc0ac5
    pop ES                                    ; 07                          ; 0xc0ac6
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ac7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0aca vgabios.c:337
    pop si                                    ; 5e                          ; 0xc0acd
    pop cx                                    ; 59                          ; 0xc0ace
    pop bp                                    ; 5d                          ; 0xc0acf
    retn                                      ; c3                          ; 0xc0ad0
  ; disGetNextSymbol 0xc0ad1 LB 0x321c -> off=0x0 cb=00000000000000a0 uValue=00000000000c0ad1 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0ad1 LB 0xa0
    push bp                                   ; 55                          ; 0xc0ad1 vgabios.c:340
    mov bp, sp                                ; 89 e5                       ; 0xc0ad2
    push bx                                   ; 53                          ; 0xc0ad4
    push cx                                   ; 51                          ; 0xc0ad5
    push si                                   ; 56                          ; 0xc0ad6
    push di                                   ; 57                          ; 0xc0ad7
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc0ad8
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0adb
    mov si, dx                                ; 89 d6                       ; 0xc0ade
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0ae0 vgabios.c:347
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ae3
    call 03188h                               ; e8 9f 26                    ; 0xc0ae6
    xor ah, ah                                ; 30 e4                       ; 0xc0ae9 vgabios.c:348
    call 03160h                               ; e8 72 26                    ; 0xc0aeb
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0aee
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0af1 vgabios.c:349
    je short 00b68h                           ; 74 73                       ; 0xc0af3
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0af5 vgabios.c:353
    xor ah, ah                                ; 30 e4                       ; 0xc0af8
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0afa
    lea dx, [bp-012h]                         ; 8d 56 ee                    ; 0xc0afd
    call 00a8bh                               ; e8 88 ff                    ; 0xc0b00
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc0b03 vgabios.c:354
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0b06
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc0b09 vgabios.c:355
    xor al, al                                ; 30 c0                       ; 0xc0b0c
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0b0e
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc0b11
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0b14 vgabios.c:358
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b17
    call 03188h                               ; e8 6b 26                    ; 0xc0b1a
    xor ah, ah                                ; 30 e4                       ; 0xc0b1d
    mov di, ax                                ; 89 c7                       ; 0xc0b1f
    inc di                                    ; 47                          ; 0xc0b21
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0b22 vgabios.c:359
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b25
    call 031a4h                               ; e8 79 26                    ; 0xc0b28
    mov cx, ax                                ; 89 c1                       ; 0xc0b2b
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc0b2d vgabios.c:361
    xor bh, bh                                ; 30 ff                       ; 0xc0b30
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0b32
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0b35
    jne short 00b68h                          ; 75 2c                       ; 0xc0b3a
    mul di                                    ; f7 e7                       ; 0xc0b3c vgabios.c:363
    add ax, ax                                ; 01 c0                       ; 0xc0b3e
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0b40
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0b42
    xor dh, dh                                ; 30 f6                       ; 0xc0b45
    inc ax                                    ; 40                          ; 0xc0b47
    mul dx                                    ; f7 e2                       ; 0xc0b48
    mov di, ax                                ; 89 c7                       ; 0xc0b4a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc0b4c
    xor ah, ah                                ; 30 e4                       ; 0xc0b4f
    mul cx                                    ; f7 e1                       ; 0xc0b51
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc0b53
    xor dh, dh                                ; 30 f6                       ; 0xc0b56
    add dx, ax                                ; 01 c2                       ; 0xc0b58
    add dx, dx                                ; 01 d2                       ; 0xc0b5a
    add dx, di                                ; 01 fa                       ; 0xc0b5c
    mov ax, word [bx+04638h]                  ; 8b 87 38 46                 ; 0xc0b5e vgabios.c:364
    call 031a4h                               ; e8 3f 26                    ; 0xc0b62
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc0b65
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0b68 vgabios.c:371
    pop di                                    ; 5f                          ; 0xc0b6b
    pop si                                    ; 5e                          ; 0xc0b6c
    pop cx                                    ; 59                          ; 0xc0b6d
    pop bx                                    ; 5b                          ; 0xc0b6e
    pop bp                                    ; 5d                          ; 0xc0b6f
    retn                                      ; c3                          ; 0xc0b70
  ; disGetNextSymbol 0xc0b71 LB 0x317c -> off=0x10 cb=000000000000007b uValue=00000000000c0b81 'vga_get_font_info'
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
    add di, ax                                ; 01 c7                       ; 0xc0b8f
    jmp word [cs:di+00b71h]                   ; 2e ff a5 71 0b              ; 0xc0b91
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc0b96 vgabios.c:380
    xor ax, ax                                ; 31 c0                       ; 0xc0b99
    call 031c0h                               ; e8 22 26                    ; 0xc0b9b
    push SS                                   ; 16                          ; 0xc0b9e vgabios.c:381
    pop ES                                    ; 07                          ; 0xc0b9f
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ba0
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc0ba3
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0ba6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ba9
    call 03188h                               ; e8 d9 25                    ; 0xc0bac
    xor ah, ah                                ; 30 e4                       ; 0xc0baf
    push SS                                   ; 16                          ; 0xc0bb1
    pop ES                                    ; 07                          ; 0xc0bb2
    mov bx, cx                                ; 89 cb                       ; 0xc0bb3
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0bb5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0bb8
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0bbb
    call 03188h                               ; e8 c7 25                    ; 0xc0bbe
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
  ; disGetNextSymbol 0xc0bfc LB 0x30f1 -> off=0x0 cb=0000000000000142 uValue=00000000000c0bfc 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0bfc LB 0x142
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
    call 03188h                               ; e8 74 25                    ; 0xc0c11
    xor ah, ah                                ; 30 e4                       ; 0xc0c14 vgabios.c:427
    call 03160h                               ; e8 47 25                    ; 0xc0c16
    mov cl, al                                ; 88 c1                       ; 0xc0c19
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0c1b vgabios.c:428
    je short 00c2dh                           ; 74 0e                       ; 0xc0c1d
    mov bl, al                                ; 88 c3                       ; 0xc0c1f vgabios.c:430
    xor bh, bh                                ; 30 ff                       ; 0xc0c21
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0c23
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0c26
    jne short 00c30h                          ; 75 03                       ; 0xc0c2b
    jmp near 00d37h                           ; e9 07 01                    ; 0xc0c2d vgabios.c:431
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc0c30 vgabios.c:434
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0c34
    jc short 00c48h                           ; 72 0f                       ; 0xc0c37
    jbe short 00c50h                          ; 76 15                       ; 0xc0c39
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0c3b
    je short 00caah                           ; 74 6a                       ; 0xc0c3e
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0c40
    je short 00c50h                           ; 74 0b                       ; 0xc0c43
    jmp near 00d32h                           ; e9 ea 00                    ; 0xc0c45
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0c48
    je short 00cafh                           ; 74 62                       ; 0xc0c4b
    jmp near 00d32h                           ; e9 e2 00                    ; 0xc0c4d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0c50 vgabios.c:437
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0c53
    call 031a4h                               ; e8 4b 25                    ; 0xc0c56
    mov bx, ax                                ; 89 c3                       ; 0xc0c59
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0c5b
    mul bx                                    ; f7 e3                       ; 0xc0c5e
    mov bx, si                                ; 89 f3                       ; 0xc0c60
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0c62
    add bx, ax                                ; 01 c3                       ; 0xc0c65
    mov cx, si                                ; 89 f1                       ; 0xc0c67 vgabios.c:438
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0c69
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0c6c
    sar ax, CL                                ; d3 f8                       ; 0xc0c6f
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0c71
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0c74 vgabios.c:440
    jmp short 00c7fh                          ; eb 06                       ; 0xc0c77
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0c79
    jnc short 00cach                          ; 73 2d                       ; 0xc0c7d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0c7f vgabios.c:441
    xor ah, ah                                ; 30 e4                       ; 0xc0c82
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0c84
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0c87
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0c89
    out DX, ax                                ; ef                          ; 0xc0c8c
    mov dx, bx                                ; 89 da                       ; 0xc0c8d vgabios.c:442
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0c8f
    call 03188h                               ; e8 f3 24                    ; 0xc0c92
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0c95
    test al, al                               ; 84 c0                       ; 0xc0c98 vgabios.c:443
    jbe short 00ca5h                          ; 76 09                       ; 0xc0c9a
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0c9c vgabios.c:444
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0c9f
    sal al, CL                                ; d2 e0                       ; 0xc0ca1
    or ch, al                                 ; 08 c5                       ; 0xc0ca3
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0ca5 vgabios.c:445
    jmp short 00c79h                          ; eb cf                       ; 0xc0ca8
    jmp short 00d11h                          ; eb 65                       ; 0xc0caa
    jmp near 00d34h                           ; e9 85 00                    ; 0xc0cac
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0caf vgabios.c:448
    shr ax, 1                                 ; d1 e8                       ; 0xc0cb2
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc0cb4
    mov bx, si                                ; 89 f3                       ; 0xc0cb7
    shr bx, 002h                              ; c1 eb 02                    ; 0xc0cb9
    add bx, ax                                ; 01 c3                       ; 0xc0cbc
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc0cbe vgabios.c:449
    je short 00cc7h                           ; 74 03                       ; 0xc0cc2
    add bh, 020h                              ; 80 c7 20                    ; 0xc0cc4 vgabios.c:450
    mov dx, bx                                ; 89 da                       ; 0xc0cc7 vgabios.c:451
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc0cc9
    call 03188h                               ; e8 b9 24                    ; 0xc0ccc
    mov bl, cl                                ; 88 cb                       ; 0xc0ccf vgabios.c:452
    xor bh, bh                                ; 30 ff                       ; 0xc0cd1
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0cd3
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc0cd6
    jne short 00cf8h                          ; 75 1b                       ; 0xc0cdb
    mov cx, si                                ; 89 f1                       ; 0xc0cdd vgabios.c:453
    xor ch, ch                                ; 30 ed                       ; 0xc0cdf
    and cl, 003h                              ; 80 e1 03                    ; 0xc0ce1
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0ce4
    sub bx, cx                                ; 29 cb                       ; 0xc0ce7
    mov cx, bx                                ; 89 d9                       ; 0xc0ce9
    add cx, bx                                ; 01 d9                       ; 0xc0ceb
    xor ah, ah                                ; 30 e4                       ; 0xc0ced
    sar ax, CL                                ; d3 f8                       ; 0xc0cef
    mov ch, al                                ; 88 c5                       ; 0xc0cf1
    and ch, 003h                              ; 80 e5 03                    ; 0xc0cf3
    jmp short 00d34h                          ; eb 3c                       ; 0xc0cf6 vgabios.c:454
    mov cx, si                                ; 89 f1                       ; 0xc0cf8 vgabios.c:455
    xor ch, ch                                ; 30 ed                       ; 0xc0cfa
    and cl, 007h                              ; 80 e1 07                    ; 0xc0cfc
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc0cff
    sub bx, cx                                ; 29 cb                       ; 0xc0d02
    mov cx, bx                                ; 89 d9                       ; 0xc0d04
    xor ah, ah                                ; 30 e4                       ; 0xc0d06
    sar ax, CL                                ; d3 f8                       ; 0xc0d08
    mov ch, al                                ; 88 c5                       ; 0xc0d0a
    and ch, 001h                              ; 80 e5 01                    ; 0xc0d0c
    jmp short 00d34h                          ; eb 23                       ; 0xc0d0f vgabios.c:456
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0d11 vgabios.c:458
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d14
    call 031a4h                               ; e8 8a 24                    ; 0xc0d17
    mov bx, ax                                ; 89 c3                       ; 0xc0d1a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0d1c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0d1f
    mul bx                                    ; f7 e3                       ; 0xc0d22
    mov dx, si                                ; 89 f2                       ; 0xc0d24
    add dx, ax                                ; 01 c2                       ; 0xc0d26
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0d28
    call 03188h                               ; e8 5a 24                    ; 0xc0d2b
    mov ch, al                                ; 88 c5                       ; 0xc0d2e
    jmp short 00d34h                          ; eb 02                       ; 0xc0d30 vgabios.c:460
    xor ch, ch                                ; 30 ed                       ; 0xc0d32 vgabios.c:465
    mov byte [ss:di], ch                      ; 36 88 2d                    ; 0xc0d34 vgabios.c:467
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d37 vgabios.c:468
    pop di                                    ; 5f                          ; 0xc0d3a
    pop si                                    ; 5e                          ; 0xc0d3b
    pop bp                                    ; 5d                          ; 0xc0d3c
    retn                                      ; c3                          ; 0xc0d3d
  ; disGetNextSymbol 0xc0d3e LB 0x2faf -> off=0x0 cb=000000000000008d uValue=00000000000c0d3e 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc0d3e LB 0x8d
    push bp                                   ; 55                          ; 0xc0d3e vgabios.c:473
    mov bp, sp                                ; 89 e5                       ; 0xc0d3f
    push bx                                   ; 53                          ; 0xc0d41
    push cx                                   ; 51                          ; 0xc0d42
    push si                                   ; 56                          ; 0xc0d43
    push di                                   ; 57                          ; 0xc0d44
    push ax                                   ; 50                          ; 0xc0d45
    push ax                                   ; 50                          ; 0xc0d46
    mov bx, ax                                ; 89 c3                       ; 0xc0d47
    mov di, dx                                ; 89 d7                       ; 0xc0d49
    mov dx, 003dah                            ; ba da 03                    ; 0xc0d4b vgabios.c:478
    in AL, DX                                 ; ec                          ; 0xc0d4e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d4f
    xor al, al                                ; 30 c0                       ; 0xc0d51 vgabios.c:479
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0d53
    out DX, AL                                ; ee                          ; 0xc0d56
    xor si, si                                ; 31 f6                       ; 0xc0d57 vgabios.c:481
    cmp si, di                                ; 39 fe                       ; 0xc0d59
    jnc short 00db0h                          ; 73 53                       ; 0xc0d5b
    mov al, bl                                ; 88 d8                       ; 0xc0d5d vgabios.c:484
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0d5f
    out DX, AL                                ; ee                          ; 0xc0d62
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0d63 vgabios.c:486
    in AL, DX                                 ; ec                          ; 0xc0d66
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d67
    mov cx, ax                                ; 89 c1                       ; 0xc0d69
    in AL, DX                                 ; ec                          ; 0xc0d6b vgabios.c:487
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d6c
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0d6e
    in AL, DX                                 ; ec                          ; 0xc0d71 vgabios.c:488
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d72
    xor ch, ch                                ; 30 ed                       ; 0xc0d74 vgabios.c:491
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc0d76
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc0d79
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc0d7c
    xor ch, ch                                ; 30 ed                       ; 0xc0d7f
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc0d81
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc0d85
    xor ah, ah                                ; 30 e4                       ; 0xc0d88
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc0d8a
    add cx, ax                                ; 01 c1                       ; 0xc0d8d
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc0d8f
    sar cx, 008h                              ; c1 f9 08                    ; 0xc0d93
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc0d96 vgabios.c:493
    jbe short 00d9eh                          ; 76 03                       ; 0xc0d99
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc0d9b
    mov al, bl                                ; 88 d8                       ; 0xc0d9e vgabios.c:496
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0da0
    out DX, AL                                ; ee                          ; 0xc0da3
    mov al, cl                                ; 88 c8                       ; 0xc0da4 vgabios.c:498
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0da6
    out DX, AL                                ; ee                          ; 0xc0da9
    out DX, AL                                ; ee                          ; 0xc0daa vgabios.c:499
    out DX, AL                                ; ee                          ; 0xc0dab vgabios.c:500
    inc bx                                    ; 43                          ; 0xc0dac vgabios.c:501
    inc si                                    ; 46                          ; 0xc0dad vgabios.c:502
    jmp short 00d59h                          ; eb a9                       ; 0xc0dae
    mov dx, 003dah                            ; ba da 03                    ; 0xc0db0 vgabios.c:503
    in AL, DX                                 ; ec                          ; 0xc0db3
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0db4
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0db6 vgabios.c:504
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0db8
    out DX, AL                                ; ee                          ; 0xc0dbb
    mov dx, 003dah                            ; ba da 03                    ; 0xc0dbc vgabios.c:506
    in AL, DX                                 ; ec                          ; 0xc0dbf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0dc0
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0dc2 vgabios.c:508
    pop di                                    ; 5f                          ; 0xc0dc5
    pop si                                    ; 5e                          ; 0xc0dc6
    pop cx                                    ; 59                          ; 0xc0dc7
    pop bx                                    ; 5b                          ; 0xc0dc8
    pop bp                                    ; 5d                          ; 0xc0dc9
    retn                                      ; c3                          ; 0xc0dca
  ; disGetNextSymbol 0xc0dcb LB 0x2f22 -> off=0x0 cb=00000000000000ae uValue=00000000000c0dcb 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc0dcb LB 0xae
    push bp                                   ; 55                          ; 0xc0dcb vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0dcc
    push bx                                   ; 53                          ; 0xc0dce
    push cx                                   ; 51                          ; 0xc0dcf
    push si                                   ; 56                          ; 0xc0dd0
    push di                                   ; 57                          ; 0xc0dd1
    push ax                                   ; 50                          ; 0xc0dd2
    push ax                                   ; 50                          ; 0xc0dd3
    mov cl, al                                ; 88 c1                       ; 0xc0dd4
    mov ch, dl                                ; 88 d5                       ; 0xc0dd6
    and cl, 03fh                              ; 80 e1 3f                    ; 0xc0dd8 vgabios.c:515
    and ch, 01fh                              ; 80 e5 1f                    ; 0xc0ddb vgabios.c:516
    mov al, cl                                ; 88 c8                       ; 0xc0dde vgabios.c:518
    xor ah, ah                                ; 30 e4                       ; 0xc0de0
    mov di, ax                                ; 89 c7                       ; 0xc0de2
    mov bx, ax                                ; 89 c3                       ; 0xc0de4
    sal bx, 008h                              ; c1 e3 08                    ; 0xc0de6
    mov al, ch                                ; 88 e8                       ; 0xc0de9
    mov si, ax                                ; 89 c6                       ; 0xc0deb
    add bx, ax                                ; 01 c3                       ; 0xc0ded
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0def vgabios.c:519
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0df2
    call 031b2h                               ; e8 ba 23                    ; 0xc0df5
    mov dx, 00089h                            ; ba 89 00                    ; 0xc0df8 vgabios.c:521
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0dfb
    call 03188h                               ; e8 87 23                    ; 0xc0dfe
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0e01
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0e04 vgabios.c:522
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e07
    call 031a4h                               ; e8 97 23                    ; 0xc0e0a
    mov bx, ax                                ; 89 c3                       ; 0xc0e0d
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0e0f
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc0e12 vgabios.c:523
    je short 00e4eh                           ; 74 36                       ; 0xc0e16
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0e18
    jbe short 00e4eh                          ; 76 31                       ; 0xc0e1b
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc0e1d
    jnc short 00e4eh                          ; 73 2c                       ; 0xc0e20
    cmp cl, 020h                              ; 80 f9 20                    ; 0xc0e22
    jnc short 00e4eh                          ; 73 27                       ; 0xc0e25
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc0e27 vgabios.c:525
    cmp si, ax                                ; 39 c6                       ; 0xc0e2a
    je short 00e36h                           ; 74 08                       ; 0xc0e2c
    mul bx                                    ; f7 e3                       ; 0xc0e2e vgabios.c:527
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e30
    dec ax                                    ; 48                          ; 0xc0e33
    jmp short 00e3eh                          ; eb 08                       ; 0xc0e34 vgabios.c:529
    inc ax                                    ; 40                          ; 0xc0e36 vgabios.c:531
    mul bx                                    ; f7 e3                       ; 0xc0e37
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e39
    dec ax                                    ; 48                          ; 0xc0e3c
    dec ax                                    ; 48                          ; 0xc0e3d
    mov cl, al                                ; 88 c1                       ; 0xc0e3e
    mov al, ch                                ; 88 e8                       ; 0xc0e40 vgabios.c:533
    xor ah, ah                                ; 30 e4                       ; 0xc0e42
    inc ax                                    ; 40                          ; 0xc0e44
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc0e45
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e48
    dec ax                                    ; 48                          ; 0xc0e4b
    mov ch, al                                ; 88 c5                       ; 0xc0e4c
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0e4e vgabios.c:537
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e51
    call 031a4h                               ; e8 4d 23                    ; 0xc0e54
    mov bx, ax                                ; 89 c3                       ; 0xc0e57
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc0e59 vgabios.c:538
    mov dx, bx                                ; 89 da                       ; 0xc0e5b
    out DX, AL                                ; ee                          ; 0xc0e5d
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc0e5e vgabios.c:539
    mov al, cl                                ; 88 c8                       ; 0xc0e61
    mov dx, si                                ; 89 f2                       ; 0xc0e63
    out DX, AL                                ; ee                          ; 0xc0e65
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc0e66 vgabios.c:540
    mov dx, bx                                ; 89 da                       ; 0xc0e68
    out DX, AL                                ; ee                          ; 0xc0e6a
    mov al, ch                                ; 88 e8                       ; 0xc0e6b vgabios.c:541
    mov dx, si                                ; 89 f2                       ; 0xc0e6d
    out DX, AL                                ; ee                          ; 0xc0e6f
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e70 vgabios.c:542
    pop di                                    ; 5f                          ; 0xc0e73
    pop si                                    ; 5e                          ; 0xc0e74
    pop cx                                    ; 59                          ; 0xc0e75
    pop bx                                    ; 5b                          ; 0xc0e76
    pop bp                                    ; 5d                          ; 0xc0e77
    retn                                      ; c3                          ; 0xc0e78
  ; disGetNextSymbol 0xc0e79 LB 0x2e74 -> off=0x0 cb=00000000000000b5 uValue=00000000000c0e79 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc0e79 LB 0xb5
    push bp                                   ; 55                          ; 0xc0e79 vgabios.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc0e7a
    push bx                                   ; 53                          ; 0xc0e7c
    push cx                                   ; 51                          ; 0xc0e7d
    push si                                   ; 56                          ; 0xc0e7e
    push ax                                   ; 50                          ; 0xc0e7f
    push ax                                   ; 50                          ; 0xc0e80
    mov cl, al                                ; 88 c1                       ; 0xc0e81
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc0e83
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0e86 vgabios.c:551
    jbe short 00e8dh                          ; 76 03                       ; 0xc0e88
    jmp near 00f26h                           ; e9 99 00                    ; 0xc0e8a
    xor ah, ah                                ; 30 e4                       ; 0xc0e8d vgabios.c:554
    mov dx, ax                                ; 89 c2                       ; 0xc0e8f
    add dx, ax                                ; 01 c2                       ; 0xc0e91
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0e93
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc0e96
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e99
    call 031b2h                               ; e8 13 23                    ; 0xc0e9c
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc0e9f vgabios.c:557
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ea2
    call 03188h                               ; e8 e0 22                    ; 0xc0ea5
    cmp cl, al                                ; 38 c1                       ; 0xc0ea8 vgabios.c:558
    jne short 00f26h                          ; 75 7a                       ; 0xc0eaa
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0eac vgabios.c:561
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eaf
    call 031a4h                               ; e8 ef 22                    ; 0xc0eb2
    mov bx, ax                                ; 89 c3                       ; 0xc0eb5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0eb7 vgabios.c:562
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eba
    call 03188h                               ; e8 c8 22                    ; 0xc0ebd
    xor ah, ah                                ; 30 e4                       ; 0xc0ec0
    mov dx, ax                                ; 89 c2                       ; 0xc0ec2
    inc dx                                    ; 42                          ; 0xc0ec4
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0ec5 vgabios.c:564
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0ec8
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0ecb
    xor al, al                                ; 30 c0                       ; 0xc0ece
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0ed0
    mov ch, al                                ; 88 c5                       ; 0xc0ed3
    mov ax, bx                                ; 89 d8                       ; 0xc0ed5 vgabios.c:567
    mul dx                                    ; f7 e2                       ; 0xc0ed7
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0ed9
    mov si, ax                                ; 89 c6                       ; 0xc0edb
    mov al, cl                                ; 88 c8                       ; 0xc0edd
    xor ah, ah                                ; 30 e4                       ; 0xc0edf
    mov dx, ax                                ; 89 c2                       ; 0xc0ee1
    lea ax, [si+001h]                         ; 8d 44 01                    ; 0xc0ee3
    mul dx                                    ; f7 e2                       ; 0xc0ee6
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc0ee8
    xor dh, dh                                ; 30 f6                       ; 0xc0eeb
    mov si, ax                                ; 89 c6                       ; 0xc0eed
    add si, dx                                ; 01 d6                       ; 0xc0eef
    mov cl, ch                                ; 88 e9                       ; 0xc0ef1
    xor ch, ch                                ; 30 ed                       ; 0xc0ef3
    mov ax, cx                                ; 89 c8                       ; 0xc0ef5
    mul bx                                    ; f7 e3                       ; 0xc0ef7
    add si, ax                                ; 01 c6                       ; 0xc0ef9
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0efb vgabios.c:570
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0efe
    call 031a4h                               ; e8 a0 22                    ; 0xc0f01
    mov bx, ax                                ; 89 c3                       ; 0xc0f04
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc0f06 vgabios.c:571
    mov dx, bx                                ; 89 da                       ; 0xc0f08
    out DX, AL                                ; ee                          ; 0xc0f0a
    mov ax, si                                ; 89 f0                       ; 0xc0f0b vgabios.c:572
    xor al, al                                ; 30 c0                       ; 0xc0f0d
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0f0f
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc0f12
    mov dx, cx                                ; 89 ca                       ; 0xc0f15
    out DX, AL                                ; ee                          ; 0xc0f17
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc0f18 vgabios.c:573
    mov dx, bx                                ; 89 da                       ; 0xc0f1a
    out DX, AL                                ; ee                          ; 0xc0f1c
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc0f1d vgabios.c:574
    mov ax, si                                ; 89 f0                       ; 0xc0f21
    mov dx, cx                                ; 89 ca                       ; 0xc0f23
    out DX, AL                                ; ee                          ; 0xc0f25
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0f26 vgabios.c:576
    pop si                                    ; 5e                          ; 0xc0f29
    pop cx                                    ; 59                          ; 0xc0f2a
    pop bx                                    ; 5b                          ; 0xc0f2b
    pop bp                                    ; 5d                          ; 0xc0f2c
    retn                                      ; c3                          ; 0xc0f2d
  ; disGetNextSymbol 0xc0f2e LB 0x2dbf -> off=0x0 cb=00000000000000ee uValue=00000000000c0f2e 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc0f2e LB 0xee
    push bp                                   ; 55                          ; 0xc0f2e vgabios.c:579
    mov bp, sp                                ; 89 e5                       ; 0xc0f2f
    push bx                                   ; 53                          ; 0xc0f31
    push cx                                   ; 51                          ; 0xc0f32
    push dx                                   ; 52                          ; 0xc0f33
    push si                                   ; 56                          ; 0xc0f34
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc0f35
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0f38
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0f3b vgabios.c:585
    jnbe short 00f53h                         ; 77 14                       ; 0xc0f3d
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0f3f vgabios.c:588
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f42
    call 03188h                               ; e8 40 22                    ; 0xc0f45
    xor ah, ah                                ; 30 e4                       ; 0xc0f48 vgabios.c:589
    call 03160h                               ; e8 13 22                    ; 0xc0f4a
    mov cl, al                                ; 88 c1                       ; 0xc0f4d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f4f vgabios.c:590
    jne short 00f56h                          ; 75 03                       ; 0xc0f51
    jmp near 01013h                           ; e9 bd 00                    ; 0xc0f53
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0f56 vgabios.c:593
    xor ah, ah                                ; 30 e4                       ; 0xc0f59
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0f5b
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc0f5e
    call 00a8bh                               ; e8 27 fb                    ; 0xc0f61
    mov bl, cl                                ; 88 cb                       ; 0xc0f64 vgabios.c:595
    xor bh, bh                                ; 30 ff                       ; 0xc0f66
    mov si, bx                                ; 89 de                       ; 0xc0f68
    sal si, 003h                              ; c1 e6 03                    ; 0xc0f6a
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc0f6d
    jne short 00fbah                          ; 75 46                       ; 0xc0f72
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0f74 vgabios.c:598
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f77
    call 031a4h                               ; e8 27 22                    ; 0xc0f7a
    mov bx, ax                                ; 89 c3                       ; 0xc0f7d
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0f7f vgabios.c:599
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f82
    call 03188h                               ; e8 00 22                    ; 0xc0f85
    xor ah, ah                                ; 30 e4                       ; 0xc0f88
    mov dx, ax                                ; 89 c2                       ; 0xc0f8a
    inc dx                                    ; 42                          ; 0xc0f8c
    mov ax, bx                                ; 89 d8                       ; 0xc0f8d vgabios.c:602
    mul dx                                    ; f7 e2                       ; 0xc0f8f
    mov cx, ax                                ; 89 c1                       ; 0xc0f91
    add ax, ax                                ; 01 c0                       ; 0xc0f93
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0f95
    mov dx, ax                                ; 89 c2                       ; 0xc0f97
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0f99
    xor ah, ah                                ; 30 e4                       ; 0xc0f9c
    mov si, ax                                ; 89 c6                       ; 0xc0f9e
    mov ax, dx                                ; 89 d0                       ; 0xc0fa0
    inc ax                                    ; 40                          ; 0xc0fa2
    mul si                                    ; f7 e6                       ; 0xc0fa3
    mov bx, ax                                ; 89 c3                       ; 0xc0fa5
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc0fa7 vgabios.c:603
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0faa
    call 031b2h                               ; e8 02 22                    ; 0xc0fad
    or cl, 0ffh                               ; 80 c9 ff                    ; 0xc0fb0 vgabios.c:606
    mov ax, cx                                ; 89 c8                       ; 0xc0fb3
    inc ax                                    ; 40                          ; 0xc0fb5
    mul si                                    ; f7 e6                       ; 0xc0fb6
    jmp short 00fd0h                          ; eb 16                       ; 0xc0fb8 vgabios.c:608
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc0fba vgabios.c:610
    xor ah, ah                                ; 30 e4                       ; 0xc0fbe
    mov bx, ax                                ; 89 c3                       ; 0xc0fc0
    sal bx, 006h                              ; c1 e3 06                    ; 0xc0fc2
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc0fc5
    xor ch, ch                                ; 30 ed                       ; 0xc0fc8
    mov ax, cx                                ; 89 c8                       ; 0xc0fca
    mul word [bx+046cbh]                      ; f7 a7 cb 46                 ; 0xc0fcc
    mov bx, ax                                ; 89 c3                       ; 0xc0fd0
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0fd2 vgabios.c:614
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fd5
    call 031a4h                               ; e8 c9 21                    ; 0xc0fd8
    mov cx, ax                                ; 89 c1                       ; 0xc0fdb
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc0fdd vgabios.c:615
    mov dx, cx                                ; 89 ca                       ; 0xc0fdf
    out DX, AL                                ; ee                          ; 0xc0fe1
    mov ax, bx                                ; 89 d8                       ; 0xc0fe2 vgabios.c:616
    xor al, bl                                ; 30 d8                       ; 0xc0fe4
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0fe6
    mov si, cx                                ; 89 ce                       ; 0xc0fe9
    inc si                                    ; 46                          ; 0xc0feb
    mov dx, si                                ; 89 f2                       ; 0xc0fec
    out DX, AL                                ; ee                          ; 0xc0fee
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc0fef vgabios.c:617
    mov dx, cx                                ; 89 ca                       ; 0xc0ff1
    out DX, AL                                ; ee                          ; 0xc0ff3
    xor bh, bh                                ; 30 ff                       ; 0xc0ff4 vgabios.c:618
    mov ax, bx                                ; 89 d8                       ; 0xc0ff6
    mov dx, si                                ; 89 f2                       ; 0xc0ff8
    out DX, AL                                ; ee                          ; 0xc0ffa
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc0ffb vgabios.c:621
    xor ch, ch                                ; 30 ed                       ; 0xc0ffe
    mov bx, cx                                ; 89 cb                       ; 0xc1000
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc1002
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1005
    call 03196h                               ; e8 8b 21                    ; 0xc1008
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc100b vgabios.c:628
    mov ax, cx                                ; 89 c8                       ; 0xc100e
    call 00e79h                               ; e8 66 fe                    ; 0xc1010
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1013 vgabios.c:629
    pop si                                    ; 5e                          ; 0xc1016
    pop dx                                    ; 5a                          ; 0xc1017
    pop cx                                    ; 59                          ; 0xc1018
    pop bx                                    ; 5b                          ; 0xc1019
    pop bp                                    ; 5d                          ; 0xc101a
    retn                                      ; c3                          ; 0xc101b
  ; disGetNextSymbol 0xc101c LB 0x2cd1 -> off=0x0 cb=00000000000003d9 uValue=00000000000c101c 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc101c LB 0x3d9
    push bp                                   ; 55                          ; 0xc101c vgabios.c:649
    mov bp, sp                                ; 89 e5                       ; 0xc101d
    push bx                                   ; 53                          ; 0xc101f
    push cx                                   ; 51                          ; 0xc1020
    push dx                                   ; 52                          ; 0xc1021
    push si                                   ; 56                          ; 0xc1022
    push di                                   ; 57                          ; 0xc1023
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc1024
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1027
    and AL, strict byte 080h                  ; 24 80                       ; 0xc102a vgabios.c:653
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc102c
    call 007bfh                               ; e8 8d f7                    ; 0xc102f vgabios.c:660
    test ax, ax                               ; 85 c0                       ; 0xc1032
    je short 01042h                           ; 74 0c                       ; 0xc1034
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1036 vgabios.c:662
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1038
    out DX, AL                                ; ee                          ; 0xc103b
    xor al, al                                ; 30 c0                       ; 0xc103c vgabios.c:663
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc103e
    out DX, AL                                ; ee                          ; 0xc1041
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc1042 vgabios.c:668
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc1046 vgabios.c:672
    jne short 01050h                          ; 75 04                       ; 0xc104a
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc104c vgabios.c:673
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1050 vgabios.c:676
    xor ah, ah                                ; 30 e4                       ; 0xc1053
    call 03160h                               ; e8 08 21                    ; 0xc1055
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1058
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc105b vgabios.c:682
    jne short 01062h                          ; 75 03                       ; 0xc105d
    jmp near 013ebh                           ; e9 89 03                    ; 0xc105f
    mov byte [bp-01ch], al                    ; 88 46 e4                    ; 0xc1062 vgabios.c:685
    mov byte [bp-01bh], 000h                  ; c6 46 e5 00                 ; 0xc1065
    mov bx, word [bp-01ch]                    ; 8b 5e e4                    ; 0xc1069
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc106c
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1070
    mov bl, al                                ; 88 c3                       ; 0xc1073 vgabios.c:686
    xor bh, bh                                ; 30 ff                       ; 0xc1075
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1077
    mov al, byte [bx+046c8h]                  ; 8a 87 c8 46                 ; 0xc107a
    xor ah, ah                                ; 30 e4                       ; 0xc107e
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1080
    mov al, byte [bx+046c9h]                  ; 8a 87 c9 46                 ; 0xc1083 vgabios.c:687
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1087
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc108a vgabios.c:688
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc108e
    mov dx, 00087h                            ; ba 87 00                    ; 0xc1091 vgabios.c:691
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1094
    call 03188h                               ; e8 ee 20                    ; 0xc1097
    mov dx, 00088h                            ; ba 88 00                    ; 0xc109a vgabios.c:694
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc109d
    call 03188h                               ; e8 e5 20                    ; 0xc10a0
    mov dx, 00089h                            ; ba 89 00                    ; 0xc10a3 vgabios.c:697
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc10a6
    call 03188h                               ; e8 dc 20                    ; 0xc10a9
    mov cl, al                                ; 88 c1                       ; 0xc10ac
    test AL, strict byte 008h                 ; a8 08                       ; 0xc10ae vgabios.c:703
    jne short 010f7h                          ; 75 45                       ; 0xc10b0
    mov bx, word [bp-01ch]                    ; 8b 5e e4                    ; 0xc10b2 vgabios.c:705
    sal bx, 003h                              ; c1 e3 03                    ; 0xc10b5
    mov al, byte [bx+0463ah]                  ; 8a 87 3a 46                 ; 0xc10b8
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc10bc
    out DX, AL                                ; ee                          ; 0xc10bf
    xor al, al                                ; 30 c0                       ; 0xc10c0 vgabios.c:708
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc10c2
    out DX, AL                                ; ee                          ; 0xc10c5
    mov bl, byte [bx+0463bh]                  ; 8a 9f 3b 46                 ; 0xc10c6 vgabios.c:711
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc10ca
    jc short 010ddh                           ; 72 0e                       ; 0xc10cd
    jbe short 010e6h                          ; 76 15                       ; 0xc10cf
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc10d1
    je short 010f0h                           ; 74 1a                       ; 0xc10d4
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc10d6
    je short 010ebh                           ; 74 10                       ; 0xc10d9
    jmp short 010f3h                          ; eb 16                       ; 0xc10db
    test bl, bl                               ; 84 db                       ; 0xc10dd
    jne short 010f3h                          ; 75 12                       ; 0xc10df
    mov di, 04e48h                            ; bf 48 4e                    ; 0xc10e1 vgabios.c:713
    jmp short 010f3h                          ; eb 0d                       ; 0xc10e4 vgabios.c:714
    mov di, 04f08h                            ; bf 08 4f                    ; 0xc10e6 vgabios.c:716
    jmp short 010f3h                          ; eb 08                       ; 0xc10e9 vgabios.c:717
    mov di, 04fc8h                            ; bf c8 4f                    ; 0xc10eb vgabios.c:719
    jmp short 010f3h                          ; eb 03                       ; 0xc10ee vgabios.c:720
    mov di, 05088h                            ; bf 88 50                    ; 0xc10f0 vgabios.c:722
    xor bx, bx                                ; 31 db                       ; 0xc10f3 vgabios.c:726
    jmp short 010ffh                          ; eb 08                       ; 0xc10f5
    jmp short 01144h                          ; eb 4b                       ; 0xc10f7
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc10f9
    jnc short 01137h                          ; 73 38                       ; 0xc10fd
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc10ff vgabios.c:727
    xor ah, ah                                ; 30 e4                       ; 0xc1102
    mov si, ax                                ; 89 c6                       ; 0xc1104
    sal si, 003h                              ; c1 e6 03                    ; 0xc1106
    mov al, byte [si+0463bh]                  ; 8a 84 3b 46                 ; 0xc1109
    mov si, ax                                ; 89 c6                       ; 0xc110d
    mov al, byte [si+046c4h]                  ; 8a 84 c4 46                 ; 0xc110f
    cmp bx, ax                                ; 39 c3                       ; 0xc1113
    jnbe short 0112ch                         ; 77 15                       ; 0xc1115
    imul si, bx, strict byte 00003h           ; 6b f3 03                    ; 0xc1117 vgabios.c:728
    add si, di                                ; 01 fe                       ; 0xc111a
    mov al, byte [si]                         ; 8a 04                       ; 0xc111c
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc111e
    out DX, AL                                ; ee                          ; 0xc1121
    mov al, byte [si+001h]                    ; 8a 44 01                    ; 0xc1122 vgabios.c:729
    out DX, AL                                ; ee                          ; 0xc1125
    mov al, byte [si+002h]                    ; 8a 44 02                    ; 0xc1126 vgabios.c:730
    out DX, AL                                ; ee                          ; 0xc1129
    jmp short 01134h                          ; eb 08                       ; 0xc112a vgabios.c:732
    xor al, al                                ; 30 c0                       ; 0xc112c vgabios.c:733
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc112e
    out DX, AL                                ; ee                          ; 0xc1131
    out DX, AL                                ; ee                          ; 0xc1132 vgabios.c:734
    out DX, AL                                ; ee                          ; 0xc1133 vgabios.c:735
    inc bx                                    ; 43                          ; 0xc1134 vgabios.c:737
    jmp short 010f9h                          ; eb c2                       ; 0xc1135
    test cl, 002h                             ; f6 c1 02                    ; 0xc1137 vgabios.c:738
    je short 01144h                           ; 74 08                       ; 0xc113a
    mov dx, 00100h                            ; ba 00 01                    ; 0xc113c vgabios.c:740
    xor ax, ax                                ; 31 c0                       ; 0xc113f
    call 00d3eh                               ; e8 fa fb                    ; 0xc1141
    mov dx, 003dah                            ; ba da 03                    ; 0xc1144 vgabios.c:745
    in AL, DX                                 ; ec                          ; 0xc1147
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1148
    xor bx, bx                                ; 31 db                       ; 0xc114a vgabios.c:748
    jmp short 01153h                          ; eb 05                       ; 0xc114c
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc114e
    jnbe short 0116dh                         ; 77 1a                       ; 0xc1151
    mov al, bl                                ; 88 d8                       ; 0xc1153 vgabios.c:749
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1155
    out DX, AL                                ; ee                          ; 0xc1158
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1159 vgabios.c:750
    xor ah, ah                                ; 30 e4                       ; 0xc115c
    mov si, ax                                ; 89 c6                       ; 0xc115e
    sal si, 006h                              ; c1 e6 06                    ; 0xc1160
    add si, bx                                ; 01 de                       ; 0xc1163
    mov al, byte [si+046ebh]                  ; 8a 84 eb 46                 ; 0xc1165
    out DX, AL                                ; ee                          ; 0xc1169
    inc bx                                    ; 43                          ; 0xc116a vgabios.c:751
    jmp short 0114eh                          ; eb e1                       ; 0xc116b
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc116d vgabios.c:752
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc116f
    out DX, AL                                ; ee                          ; 0xc1172
    xor al, al                                ; 30 c0                       ; 0xc1173 vgabios.c:753
    out DX, AL                                ; ee                          ; 0xc1175
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1176 vgabios.c:756
    out DX, AL                                ; ee                          ; 0xc1179
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc117a vgabios.c:757
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc117c
    out DX, AL                                ; ee                          ; 0xc117f
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc1180 vgabios.c:758
    jmp short 0118ah                          ; eb 05                       ; 0xc1183
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc1185
    jnbe short 011a7h                         ; 77 1d                       ; 0xc1188
    mov al, bl                                ; 88 d8                       ; 0xc118a vgabios.c:759
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc118c
    out DX, AL                                ; ee                          ; 0xc118f
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1190 vgabios.c:760
    xor ah, ah                                ; 30 e4                       ; 0xc1193
    mov si, ax                                ; 89 c6                       ; 0xc1195
    sal si, 006h                              ; c1 e6 06                    ; 0xc1197
    add si, bx                                ; 01 de                       ; 0xc119a
    mov al, byte [si+046cch]                  ; 8a 84 cc 46                 ; 0xc119c
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc11a0
    out DX, AL                                ; ee                          ; 0xc11a3
    inc bx                                    ; 43                          ; 0xc11a4 vgabios.c:761
    jmp short 01185h                          ; eb de                       ; 0xc11a5
    xor bx, bx                                ; 31 db                       ; 0xc11a7 vgabios.c:764
    jmp short 011b0h                          ; eb 05                       ; 0xc11a9
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc11ab
    jnbe short 011cdh                         ; 77 1d                       ; 0xc11ae
    mov al, bl                                ; 88 d8                       ; 0xc11b0 vgabios.c:765
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc11b2
    out DX, AL                                ; ee                          ; 0xc11b5
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc11b6 vgabios.c:766
    xor ah, ah                                ; 30 e4                       ; 0xc11b9
    mov si, ax                                ; 89 c6                       ; 0xc11bb
    sal si, 006h                              ; c1 e6 06                    ; 0xc11bd
    add si, bx                                ; 01 de                       ; 0xc11c0
    mov al, byte [si+046ffh]                  ; 8a 84 ff 46                 ; 0xc11c2
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc11c6
    out DX, AL                                ; ee                          ; 0xc11c9
    inc bx                                    ; 43                          ; 0xc11ca vgabios.c:767
    jmp short 011abh                          ; eb de                       ; 0xc11cb
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc11cd vgabios.c:770
    xor bh, bh                                ; 30 ff                       ; 0xc11d0
    sal bx, 003h                              ; c1 e3 03                    ; 0xc11d2
    cmp byte [bx+04636h], 001h                ; 80 bf 36 46 01              ; 0xc11d5
    jne short 011e1h                          ; 75 05                       ; 0xc11da
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc11dc
    jmp short 011e4h                          ; eb 03                       ; 0xc11df
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc11e1
    mov si, dx                                ; 89 d6                       ; 0xc11e4
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc11e6 vgabios.c:773
    out DX, ax                                ; ef                          ; 0xc11e9
    xor bx, bx                                ; 31 db                       ; 0xc11ea vgabios.c:775
    jmp short 011f3h                          ; eb 05                       ; 0xc11ec
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc11ee
    jnbe short 01211h                         ; 77 1e                       ; 0xc11f1
    mov al, bl                                ; 88 d8                       ; 0xc11f3 vgabios.c:776
    mov dx, si                                ; 89 f2                       ; 0xc11f5
    out DX, AL                                ; ee                          ; 0xc11f7
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc11f8 vgabios.c:777
    xor ah, ah                                ; 30 e4                       ; 0xc11fb
    mov cx, ax                                ; 89 c1                       ; 0xc11fd
    sal cx, 006h                              ; c1 e1 06                    ; 0xc11ff
    mov di, cx                                ; 89 cf                       ; 0xc1202
    add di, bx                                ; 01 df                       ; 0xc1204
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc1206
    mov al, byte [di+046d2h]                  ; 8a 85 d2 46                 ; 0xc1209
    out DX, AL                                ; ee                          ; 0xc120d
    inc bx                                    ; 43                          ; 0xc120e vgabios.c:778
    jmp short 011eeh                          ; eb dd                       ; 0xc120f
    mov bx, cx                                ; 89 cb                       ; 0xc1211 vgabios.c:781
    mov al, byte [bx+046d1h]                  ; 8a 87 d1 46                 ; 0xc1213
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1217
    out DX, AL                                ; ee                          ; 0xc121a
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc121b vgabios.c:784
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc121d
    out DX, AL                                ; ee                          ; 0xc1220
    mov dx, 003dah                            ; ba da 03                    ; 0xc1221 vgabios.c:785
    in AL, DX                                 ; ec                          ; 0xc1224
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1225
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc1227 vgabios.c:787
    jne short 0128dh                          ; 75 60                       ; 0xc122b
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc122d vgabios.c:789
    xor bh, ch                                ; 30 ef                       ; 0xc1230
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1232
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1235
    jne short 0124fh                          ; 75 13                       ; 0xc123a
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc123c vgabios.c:791
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1240
    mov ax, 00720h                            ; b8 20 07                    ; 0xc1243
    xor di, di                                ; 31 ff                       ; 0xc1246
    cld                                       ; fc                          ; 0xc1248
    jcxz 0124dh                               ; e3 02                       ; 0xc1249
    rep stosw                                 ; f3 ab                       ; 0xc124b
    jmp short 0128dh                          ; eb 3e                       ; 0xc124d vgabios.c:793
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc124f vgabios.c:795
    jnc short 01267h                          ; 73 12                       ; 0xc1253
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1255 vgabios.c:797
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1259
    xor ax, ax                                ; 31 c0                       ; 0xc125c
    xor di, di                                ; 31 ff                       ; 0xc125e
    cld                                       ; fc                          ; 0xc1260
    jcxz 01265h                               ; e3 02                       ; 0xc1261
    rep stosw                                 ; f3 ab                       ; 0xc1263
    jmp short 0128dh                          ; eb 26                       ; 0xc1265 vgabios.c:799
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc1267 vgabios.c:801
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1269
    out DX, AL                                ; ee                          ; 0xc126c
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc126d vgabios.c:802
    in AL, DX                                 ; ec                          ; 0xc1270
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1271
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1273
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1276 vgabios.c:803
    out DX, AL                                ; ee                          ; 0xc1278
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1279 vgabios.c:804
    mov cx, 08000h                            ; b9 00 80                    ; 0xc127d
    xor ax, ax                                ; 31 c0                       ; 0xc1280
    xor di, di                                ; 31 ff                       ; 0xc1282
    cld                                       ; fc                          ; 0xc1284
    jcxz 01289h                               ; e3 02                       ; 0xc1285
    rep stosw                                 ; f3 ab                       ; 0xc1287
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1289 vgabios.c:805
    out DX, AL                                ; ee                          ; 0xc128c
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc128d vgabios.c:811
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1290
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1293
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1297
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc129a
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc129d
    call 03196h                               ; e8 f3 1e                    ; 0xc12a0
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc12a3 vgabios.c:812
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc12a6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12a9
    call 031b2h                               ; e8 03 1f                    ; 0xc12ac
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc12af vgabios.c:813
    xor bh, bh                                ; 30 ff                       ; 0xc12b2
    sal bx, 006h                              ; c1 e3 06                    ; 0xc12b4
    mov bx, word [bx+046cbh]                  ; 8b 9f cb 46                 ; 0xc12b7
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc12bb
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12be
    call 031b2h                               ; e8 ee 1e                    ; 0xc12c1
    mov bx, si                                ; 89 f3                       ; 0xc12c4 vgabios.c:814
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc12c6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12c9
    call 031b2h                               ; e8 e3 1e                    ; 0xc12cc
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc12cf vgabios.c:815
    xor bh, bh                                ; 30 ff                       ; 0xc12d2
    mov dx, 00084h                            ; ba 84 00                    ; 0xc12d4
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12d7
    call 03196h                               ; e8 b9 1e                    ; 0xc12da
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc12dd vgabios.c:816
    mov dx, 00085h                            ; ba 85 00                    ; 0xc12e0
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12e3
    call 031b2h                               ; e8 c9 1e                    ; 0xc12e6
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc12e9 vgabios.c:817
    or bl, 060h                               ; 80 cb 60                    ; 0xc12ec
    xor bh, bh                                ; 30 ff                       ; 0xc12ef
    mov dx, 00087h                            ; ba 87 00                    ; 0xc12f1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12f4
    call 03196h                               ; e8 9c 1e                    ; 0xc12f7
    mov bx, 000f9h                            ; bb f9 00                    ; 0xc12fa vgabios.c:818
    mov dx, 00088h                            ; ba 88 00                    ; 0xc12fd
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1300
    call 03196h                               ; e8 90 1e                    ; 0xc1303
    mov dx, 00089h                            ; ba 89 00                    ; 0xc1306 vgabios.c:819
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1309
    call 03188h                               ; e8 79 1e                    ; 0xc130c
    mov bl, al                                ; 88 c3                       ; 0xc130f
    and bl, 07fh                              ; 80 e3 7f                    ; 0xc1311
    xor bh, bh                                ; 30 ff                       ; 0xc1314
    mov dx, 00089h                            ; ba 89 00                    ; 0xc1316
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1319
    call 03196h                               ; e8 77 1e                    ; 0xc131c
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc131f vgabios.c:822
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc1322
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1325
    call 03196h                               ; e8 6b 1e                    ; 0xc1328
    mov cx, ds                                ; 8c d9                       ; 0xc132b vgabios.c:823
    mov bx, 053d6h                            ; bb d6 53                    ; 0xc132d
    mov dx, 000a8h                            ; ba a8 00                    ; 0xc1330
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1333
    call 031d2h                               ; e8 99 1e                    ; 0xc1336
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc1339 vgabios.c:825
    jnbe short 0136ah                         ; 77 2b                       ; 0xc133d
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc133f vgabios.c:827
    mov bl, byte [bx+07c63h]                  ; 8a 9f 63 7c                 ; 0xc1342
    xor bh, bh                                ; 30 ff                       ; 0xc1346
    mov dx, strict word 00065h                ; ba 65 00                    ; 0xc1348
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc134b
    call 03196h                               ; e8 45 1e                    ; 0xc134e
    cmp byte [bp-00ch], 006h                  ; 80 7e f4 06                 ; 0xc1351 vgabios.c:828
    jne short 0135ch                          ; 75 05                       ; 0xc1355
    mov bx, strict word 0003fh                ; bb 3f 00                    ; 0xc1357
    jmp short 0135fh                          ; eb 03                       ; 0xc135a
    mov bx, strict word 00030h                ; bb 30 00                    ; 0xc135c
    xor bh, bh                                ; 30 ff                       ; 0xc135f
    mov dx, strict word 00066h                ; ba 66 00                    ; 0xc1361
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1364
    call 03196h                               ; e8 2c 1e                    ; 0xc1367
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc136a vgabios.c:832
    xor bh, bh                                ; 30 ff                       ; 0xc136d
    sal bx, 003h                              ; c1 e3 03                    ; 0xc136f
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1372
    jne short 01382h                          ; 75 09                       ; 0xc1377
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc1379 vgabios.c:834
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc137c
    call 00dcbh                               ; e8 49 fa                    ; 0xc137f
    xor bx, bx                                ; 31 db                       ; 0xc1382 vgabios.c:838
    jmp short 0138bh                          ; eb 05                       ; 0xc1384
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1386
    jnc short 01397h                          ; 73 0c                       ; 0xc1389
    mov al, bl                                ; 88 d8                       ; 0xc138b vgabios.c:839
    xor ah, ah                                ; 30 e4                       ; 0xc138d
    xor dx, dx                                ; 31 d2                       ; 0xc138f
    call 00e79h                               ; e8 e5 fa                    ; 0xc1391
    inc bx                                    ; 43                          ; 0xc1394
    jmp short 01386h                          ; eb ef                       ; 0xc1395
    xor ax, ax                                ; 31 c0                       ; 0xc1397 vgabios.c:842
    call 00f2eh                               ; e8 92 fb                    ; 0xc1399
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc139c vgabios.c:845
    xor bh, bh                                ; 30 ff                       ; 0xc139f
    sal bx, 003h                              ; c1 e3 03                    ; 0xc13a1
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc13a4
    jne short 013bbh                          ; 75 10                       ; 0xc13a9
    xor bl, bl                                ; 30 db                       ; 0xc13ab vgabios.c:847
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc13ad
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc13af
    int 010h                                  ; cd 10                       ; 0xc13b1
    xor bl, bl                                ; 30 db                       ; 0xc13b3 vgabios.c:848
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc13b5
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc13b7
    int 010h                                  ; cd 10                       ; 0xc13b9
    mov dx, 057f2h                            ; ba f2 57                    ; 0xc13bb vgabios.c:852
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc13be
    call 00a00h                               ; e8 3c f6                    ; 0xc13c1
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc13c4 vgabios.c:854
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc13c7
    je short 013e6h                           ; 74 1a                       ; 0xc13ca
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc13cc
    je short 013e1h                           ; 74 10                       ; 0xc13cf
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc13d1
    jne short 013ebh                          ; 75 15                       ; 0xc13d4
    mov dx, 053f2h                            ; ba f2 53                    ; 0xc13d6 vgabios.c:856
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc13d9
    call 00a00h                               ; e8 21 f6                    ; 0xc13dc
    jmp short 013ebh                          ; eb 0a                       ; 0xc13df vgabios.c:857
    mov dx, 05bf2h                            ; ba f2 5b                    ; 0xc13e1 vgabios.c:859
    jmp short 013d9h                          ; eb f3                       ; 0xc13e4
    mov dx, 069f2h                            ; ba f2 69                    ; 0xc13e6 vgabios.c:862
    jmp short 013d9h                          ; eb ee                       ; 0xc13e9
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc13eb vgabios.c:865
    pop di                                    ; 5f                          ; 0xc13ee
    pop si                                    ; 5e                          ; 0xc13ef
    pop dx                                    ; 5a                          ; 0xc13f0
    pop cx                                    ; 59                          ; 0xc13f1
    pop bx                                    ; 5b                          ; 0xc13f2
    pop bp                                    ; 5d                          ; 0xc13f3
    retn                                      ; c3                          ; 0xc13f4
  ; disGetNextSymbol 0xc13f5 LB 0x28f8 -> off=0x0 cb=000000000000008f uValue=00000000000c13f5 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc13f5 LB 0x8f
    push bp                                   ; 55                          ; 0xc13f5 vgabios.c:868
    mov bp, sp                                ; 89 e5                       ; 0xc13f6
    push si                                   ; 56                          ; 0xc13f8
    push di                                   ; 57                          ; 0xc13f9
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc13fa
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc13fd
    mov al, dl                                ; 88 d0                       ; 0xc1400
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1402
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1405
    xor ah, ah                                ; 30 e4                       ; 0xc1408 vgabios.c:874
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc140a
    xor dh, dh                                ; 30 f6                       ; 0xc140d
    mov cx, dx                                ; 89 d1                       ; 0xc140f
    imul dx                                   ; f7 ea                       ; 0xc1411
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1413
    xor dh, dh                                ; 30 f6                       ; 0xc1416
    mov si, dx                                ; 89 d6                       ; 0xc1418
    imul dx                                   ; f7 ea                       ; 0xc141a
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc141c
    xor dh, dh                                ; 30 f6                       ; 0xc141f
    mov bx, dx                                ; 89 d3                       ; 0xc1421
    add ax, dx                                ; 01 d0                       ; 0xc1423
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1425
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1428 vgabios.c:875
    xor ah, ah                                ; 30 e4                       ; 0xc142b
    imul cx                                   ; f7 e9                       ; 0xc142d
    imul si                                   ; f7 ee                       ; 0xc142f
    add ax, bx                                ; 01 d8                       ; 0xc1431
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1433
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1436 vgabios.c:876
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1439
    out DX, ax                                ; ef                          ; 0xc143c
    xor bl, bl                                ; 30 db                       ; 0xc143d vgabios.c:877
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc143f
    jnc short 01474h                          ; 73 30                       ; 0xc1442
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1444 vgabios.c:879
    xor ah, ah                                ; 30 e4                       ; 0xc1447
    mov cx, ax                                ; 89 c1                       ; 0xc1449
    mov al, bl                                ; 88 d8                       ; 0xc144b
    mov dx, ax                                ; 89 c2                       ; 0xc144d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc144f
    mov si, ax                                ; 89 c6                       ; 0xc1452
    mov ax, dx                                ; 89 d0                       ; 0xc1454
    imul si                                   ; f7 ee                       ; 0xc1456
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1458
    add si, ax                                ; 01 c6                       ; 0xc145b
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc145d
    add di, ax                                ; 01 c7                       ; 0xc1460
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1462
    mov es, dx                                ; 8e c2                       ; 0xc1465
    cld                                       ; fc                          ; 0xc1467
    jcxz 01470h                               ; e3 06                       ; 0xc1468
    push DS                                   ; 1e                          ; 0xc146a
    mov ds, dx                                ; 8e da                       ; 0xc146b
    rep movsb                                 ; f3 a4                       ; 0xc146d
    pop DS                                    ; 1f                          ; 0xc146f
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1470 vgabios.c:880
    jmp short 0143fh                          ; eb cb                       ; 0xc1472
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1474 vgabios.c:881
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1477
    out DX, ax                                ; ef                          ; 0xc147a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc147b vgabios.c:882
    pop di                                    ; 5f                          ; 0xc147e
    pop si                                    ; 5e                          ; 0xc147f
    pop bp                                    ; 5d                          ; 0xc1480
    retn 00004h                               ; c2 04 00                    ; 0xc1481
  ; disGetNextSymbol 0xc1484 LB 0x2869 -> off=0x0 cb=000000000000007c uValue=00000000000c1484 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc1484 LB 0x7c
    push bp                                   ; 55                          ; 0xc1484 vgabios.c:885
    mov bp, sp                                ; 89 e5                       ; 0xc1485
    push si                                   ; 56                          ; 0xc1487
    push di                                   ; 57                          ; 0xc1488
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1489
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc148c
    mov al, dl                                ; 88 d0                       ; 0xc148f
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1491
    mov bh, cl                                ; 88 cf                       ; 0xc1494
    xor ah, ah                                ; 30 e4                       ; 0xc1496 vgabios.c:891
    mov dx, ax                                ; 89 c2                       ; 0xc1498
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc149a
    mov cx, ax                                ; 89 c1                       ; 0xc149d
    mov ax, dx                                ; 89 d0                       ; 0xc149f
    imul cx                                   ; f7 e9                       ; 0xc14a1
    mov dl, bh                                ; 88 fa                       ; 0xc14a3
    xor dh, dh                                ; 30 f6                       ; 0xc14a5
    imul dx                                   ; f7 ea                       ; 0xc14a7
    mov dx, ax                                ; 89 c2                       ; 0xc14a9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc14ab
    xor ah, ah                                ; 30 e4                       ; 0xc14ae
    add dx, ax                                ; 01 c2                       ; 0xc14b0
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc14b2
    mov ax, 00205h                            ; b8 05 02                    ; 0xc14b5 vgabios.c:892
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc14b8
    out DX, ax                                ; ef                          ; 0xc14bb
    xor bl, bl                                ; 30 db                       ; 0xc14bc vgabios.c:893
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc14be
    jnc short 014f0h                          ; 73 2d                       ; 0xc14c1
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc14c3 vgabios.c:895
    xor ch, ch                                ; 30 ed                       ; 0xc14c6
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc14c8
    xor ah, ah                                ; 30 e4                       ; 0xc14cb
    mov si, ax                                ; 89 c6                       ; 0xc14cd
    mov al, bl                                ; 88 d8                       ; 0xc14cf
    mov dx, ax                                ; 89 c2                       ; 0xc14d1
    mov al, bh                                ; 88 f8                       ; 0xc14d3
    mov di, ax                                ; 89 c7                       ; 0xc14d5
    mov ax, dx                                ; 89 d0                       ; 0xc14d7
    imul di                                   ; f7 ef                       ; 0xc14d9
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc14db
    add di, ax                                ; 01 c7                       ; 0xc14de
    mov ax, si                                ; 89 f0                       ; 0xc14e0
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc14e2
    mov es, dx                                ; 8e c2                       ; 0xc14e5
    cld                                       ; fc                          ; 0xc14e7
    jcxz 014ech                               ; e3 02                       ; 0xc14e8
    rep stosb                                 ; f3 aa                       ; 0xc14ea
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc14ec vgabios.c:896
    jmp short 014beh                          ; eb ce                       ; 0xc14ee
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc14f0 vgabios.c:897
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc14f3
    out DX, ax                                ; ef                          ; 0xc14f6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc14f7 vgabios.c:898
    pop di                                    ; 5f                          ; 0xc14fa
    pop si                                    ; 5e                          ; 0xc14fb
    pop bp                                    ; 5d                          ; 0xc14fc
    retn 00004h                               ; c2 04 00                    ; 0xc14fd
  ; disGetNextSymbol 0xc1500 LB 0x27ed -> off=0x0 cb=00000000000000c2 uValue=00000000000c1500 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1500 LB 0xc2
    push bp                                   ; 55                          ; 0xc1500 vgabios.c:901
    mov bp, sp                                ; 89 e5                       ; 0xc1501
    push si                                   ; 56                          ; 0xc1503
    push di                                   ; 57                          ; 0xc1504
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1505
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1508
    mov al, dl                                ; 88 d0                       ; 0xc150b
    mov bh, cl                                ; 88 cf                       ; 0xc150d
    xor ah, ah                                ; 30 e4                       ; 0xc150f vgabios.c:907
    mov dx, ax                                ; 89 c2                       ; 0xc1511
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1513
    mov cx, ax                                ; 89 c1                       ; 0xc1516
    mov ax, dx                                ; 89 d0                       ; 0xc1518
    imul cx                                   ; f7 e9                       ; 0xc151a
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc151c
    xor dh, dh                                ; 30 f6                       ; 0xc151f
    mov di, dx                                ; 89 d7                       ; 0xc1521
    imul dx                                   ; f7 ea                       ; 0xc1523
    mov dx, ax                                ; 89 c2                       ; 0xc1525
    sar dx, 1                                 ; d1 fa                       ; 0xc1527
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1529
    xor ah, ah                                ; 30 e4                       ; 0xc152c
    mov si, ax                                ; 89 c6                       ; 0xc152e
    add dx, ax                                ; 01 c2                       ; 0xc1530
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc1532
    mov al, bl                                ; 88 d8                       ; 0xc1535 vgabios.c:908
    imul cx                                   ; f7 e9                       ; 0xc1537
    imul di                                   ; f7 ef                       ; 0xc1539
    sar ax, 1                                 ; d1 f8                       ; 0xc153b
    add ax, si                                ; 01 f0                       ; 0xc153d
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc153f
    xor bl, bl                                ; 30 db                       ; 0xc1542 vgabios.c:909
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1544
    jnc short 015b9h                          ; 73 70                       ; 0xc1547
    test bl, 001h                             ; f6 c3 01                    ; 0xc1549 vgabios.c:911
    je short 01585h                           ; 74 37                       ; 0xc154c
    mov cl, bh                                ; 88 f9                       ; 0xc154e vgabios.c:912
    xor ch, ch                                ; 30 ed                       ; 0xc1550
    mov al, bl                                ; 88 d8                       ; 0xc1552
    xor ah, ah                                ; 30 e4                       ; 0xc1554
    mov dx, ax                                ; 89 c2                       ; 0xc1556
    sar dx, 1                                 ; d1 fa                       ; 0xc1558
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc155a
    mov si, ax                                ; 89 c6                       ; 0xc155d
    mov ax, dx                                ; 89 d0                       ; 0xc155f
    imul si                                   ; f7 ee                       ; 0xc1561
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1563
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc1566
    add si, ax                                ; 01 c6                       ; 0xc156a
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc156c
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc156f
    add di, ax                                ; 01 c7                       ; 0xc1573
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1575
    mov es, dx                                ; 8e c2                       ; 0xc1578
    cld                                       ; fc                          ; 0xc157a
    jcxz 01583h                               ; e3 06                       ; 0xc157b
    push DS                                   ; 1e                          ; 0xc157d
    mov ds, dx                                ; 8e da                       ; 0xc157e
    rep movsb                                 ; f3 a4                       ; 0xc1580
    pop DS                                    ; 1f                          ; 0xc1582
    jmp short 015b5h                          ; eb 30                       ; 0xc1583 vgabios.c:913
    mov al, bh                                ; 88 f8                       ; 0xc1585 vgabios.c:914
    xor ah, ah                                ; 30 e4                       ; 0xc1587
    mov cx, ax                                ; 89 c1                       ; 0xc1589
    mov al, bl                                ; 88 d8                       ; 0xc158b
    sar ax, 1                                 ; d1 f8                       ; 0xc158d
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc158f
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc1592
    mov byte [bp-00bh], ch                    ; 88 6e f5                    ; 0xc1595
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1598
    imul dx                                   ; f7 ea                       ; 0xc159b
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc159d
    add si, ax                                ; 01 c6                       ; 0xc15a0
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc15a2
    add di, ax                                ; 01 c7                       ; 0xc15a5
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc15a7
    mov es, dx                                ; 8e c2                       ; 0xc15aa
    cld                                       ; fc                          ; 0xc15ac
    jcxz 015b5h                               ; e3 06                       ; 0xc15ad
    push DS                                   ; 1e                          ; 0xc15af
    mov ds, dx                                ; 8e da                       ; 0xc15b0
    rep movsb                                 ; f3 a4                       ; 0xc15b2
    pop DS                                    ; 1f                          ; 0xc15b4
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc15b5 vgabios.c:915
    jmp short 01544h                          ; eb 8b                       ; 0xc15b7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc15b9 vgabios.c:916
    pop di                                    ; 5f                          ; 0xc15bc
    pop si                                    ; 5e                          ; 0xc15bd
    pop bp                                    ; 5d                          ; 0xc15be
    retn 00004h                               ; c2 04 00                    ; 0xc15bf
  ; disGetNextSymbol 0xc15c2 LB 0x272b -> off=0x0 cb=00000000000000a8 uValue=00000000000c15c2 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc15c2 LB 0xa8
    push bp                                   ; 55                          ; 0xc15c2 vgabios.c:919
    mov bp, sp                                ; 89 e5                       ; 0xc15c3
    push si                                   ; 56                          ; 0xc15c5
    push di                                   ; 57                          ; 0xc15c6
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc15c7
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc15ca
    mov al, dl                                ; 88 d0                       ; 0xc15cd
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc15cf
    mov bh, cl                                ; 88 cf                       ; 0xc15d2
    xor ah, ah                                ; 30 e4                       ; 0xc15d4 vgabios.c:925
    mov dx, ax                                ; 89 c2                       ; 0xc15d6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc15d8
    mov cx, ax                                ; 89 c1                       ; 0xc15db
    mov ax, dx                                ; 89 d0                       ; 0xc15dd
    imul cx                                   ; f7 e9                       ; 0xc15df
    mov dl, bh                                ; 88 fa                       ; 0xc15e1
    xor dh, dh                                ; 30 f6                       ; 0xc15e3
    imul dx                                   ; f7 ea                       ; 0xc15e5
    mov dx, ax                                ; 89 c2                       ; 0xc15e7
    sar dx, 1                                 ; d1 fa                       ; 0xc15e9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc15eb
    xor ah, ah                                ; 30 e4                       ; 0xc15ee
    add dx, ax                                ; 01 c2                       ; 0xc15f0
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc15f2
    xor bl, bl                                ; 30 db                       ; 0xc15f5 vgabios.c:926
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc15f7
    jnc short 01661h                          ; 73 65                       ; 0xc15fa
    test bl, 001h                             ; f6 c3 01                    ; 0xc15fc vgabios.c:928
    je short 01632h                           ; 74 31                       ; 0xc15ff
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1601 vgabios.c:929
    xor ah, ah                                ; 30 e4                       ; 0xc1604
    mov cx, ax                                ; 89 c1                       ; 0xc1606
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1608
    mov si, ax                                ; 89 c6                       ; 0xc160b
    mov al, bl                                ; 88 d8                       ; 0xc160d
    mov dx, ax                                ; 89 c2                       ; 0xc160f
    sar dx, 1                                 ; d1 fa                       ; 0xc1611
    mov al, bh                                ; 88 f8                       ; 0xc1613
    mov di, ax                                ; 89 c7                       ; 0xc1615
    mov ax, dx                                ; 89 d0                       ; 0xc1617
    imul di                                   ; f7 ef                       ; 0xc1619
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc161b
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc161e
    add di, ax                                ; 01 c7                       ; 0xc1622
    mov ax, si                                ; 89 f0                       ; 0xc1624
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1626
    mov es, dx                                ; 8e c2                       ; 0xc1629
    cld                                       ; fc                          ; 0xc162b
    jcxz 01630h                               ; e3 02                       ; 0xc162c
    rep stosb                                 ; f3 aa                       ; 0xc162e
    jmp short 0165dh                          ; eb 2b                       ; 0xc1630 vgabios.c:930
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1632 vgabios.c:931
    xor ah, ah                                ; 30 e4                       ; 0xc1635
    mov cx, ax                                ; 89 c1                       ; 0xc1637
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1639
    mov si, ax                                ; 89 c6                       ; 0xc163c
    mov al, bl                                ; 88 d8                       ; 0xc163e
    mov dx, ax                                ; 89 c2                       ; 0xc1640
    sar dx, 1                                 ; d1 fa                       ; 0xc1642
    mov al, bh                                ; 88 f8                       ; 0xc1644
    mov di, ax                                ; 89 c7                       ; 0xc1646
    mov ax, dx                                ; 89 d0                       ; 0xc1648
    imul di                                   ; f7 ef                       ; 0xc164a
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc164c
    add di, ax                                ; 01 c7                       ; 0xc164f
    mov ax, si                                ; 89 f0                       ; 0xc1651
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1653
    mov es, dx                                ; 8e c2                       ; 0xc1656
    cld                                       ; fc                          ; 0xc1658
    jcxz 0165dh                               ; e3 02                       ; 0xc1659
    rep stosb                                 ; f3 aa                       ; 0xc165b
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc165d vgabios.c:932
    jmp short 015f7h                          ; eb 96                       ; 0xc165f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1661 vgabios.c:933
    pop di                                    ; 5f                          ; 0xc1664
    pop si                                    ; 5e                          ; 0xc1665
    pop bp                                    ; 5d                          ; 0xc1666
    retn 00004h                               ; c2 04 00                    ; 0xc1667
  ; disGetNextSymbol 0xc166a LB 0x2683 -> off=0x0 cb=000000000000055a uValue=00000000000c166a 'biosfn_scroll'
biosfn_scroll:                               ; 0xc166a LB 0x55a
    push bp                                   ; 55                          ; 0xc166a vgabios.c:936
    mov bp, sp                                ; 89 e5                       ; 0xc166b
    push si                                   ; 56                          ; 0xc166d
    push di                                   ; 57                          ; 0xc166e
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc166f
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1672
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1675
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1678
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc167b
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc167e vgabios.c:945
    jnbe short 0169dh                         ; 77 1a                       ; 0xc1681
    cmp cl, byte [bp+006h]                    ; 3a 4e 06                    ; 0xc1683 vgabios.c:946
    jnbe short 0169dh                         ; 77 15                       ; 0xc1686
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1688 vgabios.c:949
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc168b
    call 03188h                               ; e8 f7 1a                    ; 0xc168e
    xor ah, ah                                ; 30 e4                       ; 0xc1691 vgabios.c:950
    call 03160h                               ; e8 ca 1a                    ; 0xc1693
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1696
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1699 vgabios.c:951
    jne short 016a0h                          ; 75 03                       ; 0xc169b
    jmp near 01bbbh                           ; e9 1b 05                    ; 0xc169d
    mov dx, 00084h                            ; ba 84 00                    ; 0xc16a0 vgabios.c:954
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16a3
    call 03188h                               ; e8 df 1a                    ; 0xc16a6
    xor ah, ah                                ; 30 e4                       ; 0xc16a9
    mov cx, ax                                ; 89 c1                       ; 0xc16ab
    inc cx                                    ; 41                          ; 0xc16ad
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc16ae vgabios.c:955
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16b1
    call 031a4h                               ; e8 ed 1a                    ; 0xc16b4
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc16b7
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc16ba vgabios.c:958
    jne short 016cch                          ; 75 0c                       ; 0xc16be
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc16c0 vgabios.c:959
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16c3
    call 03188h                               ; e8 bf 1a                    ; 0xc16c6
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc16c9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc16cc vgabios.c:961
    xor ah, ah                                ; 30 e4                       ; 0xc16cf
    cmp ax, cx                                ; 39 c8                       ; 0xc16d1
    jc short 016dch                           ; 72 07                       ; 0xc16d3
    mov al, cl                                ; 88 c8                       ; 0xc16d5
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc16d7
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc16d9
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc16dc vgabios.c:962
    xor ah, ah                                ; 30 e4                       ; 0xc16df
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc16e1
    jc short 016eeh                           ; 72 08                       ; 0xc16e4
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc16e6
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc16e9
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc16eb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc16ee vgabios.c:963
    xor ah, ah                                ; 30 e4                       ; 0xc16f1
    cmp ax, cx                                ; 39 c8                       ; 0xc16f3
    jbe short 016fah                          ; 76 03                       ; 0xc16f5
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc16f7
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc16fa vgabios.c:964
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc16fd
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1700
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1702
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1705 vgabios.c:966
    xor bh, bh                                ; 30 ff                       ; 0xc1708
    mov di, bx                                ; 89 df                       ; 0xc170a
    sal di, 003h                              ; c1 e7 03                    ; 0xc170c
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc170f
    dec ax                                    ; 48                          ; 0xc1712
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1713
    mov ax, cx                                ; 89 c8                       ; 0xc1716
    dec ax                                    ; 48                          ; 0xc1718
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1719
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc171c
    mul cx                                    ; f7 e1                       ; 0xc171f
    mov si, ax                                ; 89 c6                       ; 0xc1721
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1723
    jne short 0177bh                          ; 75 51                       ; 0xc1728
    add ax, ax                                ; 01 c0                       ; 0xc172a vgabios.c:969
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc172c
    mov dx, ax                                ; 89 c2                       ; 0xc172e
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc1730
    xor ah, ah                                ; 30 e4                       ; 0xc1733
    mov bx, ax                                ; 89 c3                       ; 0xc1735
    mov ax, dx                                ; 89 d0                       ; 0xc1737
    inc ax                                    ; 40                          ; 0xc1739
    mul bx                                    ; f7 e3                       ; 0xc173a
    mov bx, ax                                ; 89 c3                       ; 0xc173c
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc173e vgabios.c:974
    jne short 0177eh                          ; 75 3a                       ; 0xc1742
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1744
    jne short 0177eh                          ; 75 34                       ; 0xc1748
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc174a
    jne short 0177eh                          ; 75 2e                       ; 0xc174e
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1750
    xor ah, ah                                ; 30 e4                       ; 0xc1753
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1755
    jne short 0177eh                          ; 75 24                       ; 0xc1758
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc175a
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc175d
    jne short 0177eh                          ; 75 1c                       ; 0xc1760
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1762 vgabios.c:976
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1765
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1768
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc176b
    mov cx, si                                ; 89 f1                       ; 0xc176f
    mov di, bx                                ; 89 df                       ; 0xc1771
    cld                                       ; fc                          ; 0xc1773
    jcxz 01778h                               ; e3 02                       ; 0xc1774
    rep stosw                                 ; f3 ab                       ; 0xc1776
    jmp near 01bbbh                           ; e9 40 04                    ; 0xc1778 vgabios.c:978
    jmp near 018f9h                           ; e9 7b 01                    ; 0xc177b
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc177e vgabios.c:980
    jne short 017e8h                          ; 75 64                       ; 0xc1782
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1784 vgabios.c:981
    xor ah, ah                                ; 30 e4                       ; 0xc1787
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1789
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc178c
    xor dh, dh                                ; 30 f6                       ; 0xc178f
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1791
    jc short 017eah                           ; 72 54                       ; 0xc1794
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1796 vgabios.c:983
    xor ah, ah                                ; 30 e4                       ; 0xc1799
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc179b
    cmp ax, dx                                ; 39 d0                       ; 0xc179e
    jnbe short 017a8h                         ; 77 06                       ; 0xc17a0
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc17a2
    jne short 017edh                          ; 75 45                       ; 0xc17a6
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc17a8 vgabios.c:984
    xor ah, ah                                ; 30 e4                       ; 0xc17ab
    mov cx, ax                                ; 89 c1                       ; 0xc17ad
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc17af
    sal ax, 008h                              ; c1 e0 08                    ; 0xc17b2
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc17b5
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc17b8
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc17bb
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc17be
    mov dx, ax                                ; 89 c2                       ; 0xc17c1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17c3
    xor ah, ah                                ; 30 e4                       ; 0xc17c6
    add ax, dx                                ; 01 d0                       ; 0xc17c8
    add ax, ax                                ; 01 c0                       ; 0xc17ca
    mov di, bx                                ; 89 df                       ; 0xc17cc
    add di, ax                                ; 01 c7                       ; 0xc17ce
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc17d0
    xor ah, ah                                ; 30 e4                       ; 0xc17d3
    mov si, ax                                ; 89 c6                       ; 0xc17d5
    sal si, 003h                              ; c1 e6 03                    ; 0xc17d7
    mov es, [si+04638h]                       ; 8e 84 38 46                 ; 0xc17da
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc17de
    cld                                       ; fc                          ; 0xc17e1
    jcxz 017e6h                               ; e3 02                       ; 0xc17e2
    rep stosw                                 ; f3 ab                       ; 0xc17e4
    jmp short 01831h                          ; eb 49                       ; 0xc17e6 vgabios.c:985
    jmp short 01837h                          ; eb 4d                       ; 0xc17e8
    jmp near 01bbbh                           ; e9 ce 03                    ; 0xc17ea
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc17ed vgabios.c:986
    mov cx, dx                                ; 89 d1                       ; 0xc17f0
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc17f2
    mov dx, ax                                ; 89 c2                       ; 0xc17f5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17f7
    xor ah, ah                                ; 30 e4                       ; 0xc17fa
    mov di, ax                                ; 89 c7                       ; 0xc17fc
    add dx, ax                                ; 01 c2                       ; 0xc17fe
    add dx, dx                                ; 01 d2                       ; 0xc1800
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc1802
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1805
    mov si, ax                                ; 89 c6                       ; 0xc1808
    sal si, 003h                              ; c1 e6 03                    ; 0xc180a
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc180d
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1811
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1814
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1817
    add di, ax                                ; 01 c7                       ; 0xc181a
    add di, di                                ; 01 ff                       ; 0xc181c
    add di, bx                                ; 01 df                       ; 0xc181e
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc1820
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc1823
    mov es, dx                                ; 8e c2                       ; 0xc1826
    cld                                       ; fc                          ; 0xc1828
    jcxz 01831h                               ; e3 06                       ; 0xc1829
    push DS                                   ; 1e                          ; 0xc182b
    mov ds, dx                                ; 8e da                       ; 0xc182c
    rep movsw                                 ; f3 a5                       ; 0xc182e
    pop DS                                    ; 1f                          ; 0xc1830
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1831 vgabios.c:987
    jmp near 0178ch                           ; e9 55 ff                    ; 0xc1834
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1837 vgabios.c:990
    xor ah, ah                                ; 30 e4                       ; 0xc183a
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc183c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc183f
    xor ah, ah                                ; 30 e4                       ; 0xc1842
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1844
    jnbe short 017eah                         ; 77 a1                       ; 0xc1847
    mov dx, ax                                ; 89 c2                       ; 0xc1849 vgabios.c:992
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc184b
    add dx, ax                                ; 01 c2                       ; 0xc184e
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1850
    jnbe short 01859h                         ; 77 04                       ; 0xc1853
    test al, al                               ; 84 c0                       ; 0xc1855
    jne short 01897h                          ; 75 3e                       ; 0xc1857
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1859 vgabios.c:993
    xor ah, ah                                ; 30 e4                       ; 0xc185c
    mov cx, ax                                ; 89 c1                       ; 0xc185e
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1860
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1863
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1866
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1869
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc186c
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc186f
    mov dx, ax                                ; 89 c2                       ; 0xc1872
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1874
    xor ah, ah                                ; 30 e4                       ; 0xc1877
    add dx, ax                                ; 01 c2                       ; 0xc1879
    add dx, dx                                ; 01 d2                       ; 0xc187b
    mov di, bx                                ; 89 df                       ; 0xc187d
    add di, dx                                ; 01 d7                       ; 0xc187f
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1881
    mov si, ax                                ; 89 c6                       ; 0xc1884
    sal si, 003h                              ; c1 e6 03                    ; 0xc1886
    mov es, [si+04638h]                       ; 8e 84 38 46                 ; 0xc1889
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc188d
    cld                                       ; fc                          ; 0xc1890
    jcxz 01895h                               ; e3 02                       ; 0xc1891
    rep stosw                                 ; f3 ab                       ; 0xc1893
    jmp short 018e9h                          ; eb 52                       ; 0xc1895 vgabios.c:994
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1897 vgabios.c:995
    mov di, ax                                ; 89 c7                       ; 0xc189a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc189c
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc189f
    sub dx, ax                                ; 29 c2                       ; 0xc18a2
    mov ax, dx                                ; 89 d0                       ; 0xc18a4
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc18a6
    mov dx, ax                                ; 89 c2                       ; 0xc18a9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc18ab
    xor ah, ah                                ; 30 e4                       ; 0xc18ae
    mov cx, ax                                ; 89 c1                       ; 0xc18b0
    add dx, ax                                ; 01 c2                       ; 0xc18b2
    add dx, dx                                ; 01 d2                       ; 0xc18b4
    mov word [bp-01eh], dx                    ; 89 56 e2                    ; 0xc18b6
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc18b9
    mov si, ax                                ; 89 c6                       ; 0xc18bc
    sal si, 003h                              ; c1 e6 03                    ; 0xc18be
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc18c1
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc18c5
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc18c8
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc18cb
    add ax, cx                                ; 01 c8                       ; 0xc18ce
    add ax, ax                                ; 01 c0                       ; 0xc18d0
    add ax, bx                                ; 01 d8                       ; 0xc18d2
    mov cx, di                                ; 89 f9                       ; 0xc18d4
    mov si, word [bp-01eh]                    ; 8b 76 e2                    ; 0xc18d6
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc18d9
    mov di, ax                                ; 89 c7                       ; 0xc18dc
    mov es, dx                                ; 8e c2                       ; 0xc18de
    cld                                       ; fc                          ; 0xc18e0
    jcxz 018e9h                               ; e3 06                       ; 0xc18e1
    push DS                                   ; 1e                          ; 0xc18e3
    mov ds, dx                                ; 8e da                       ; 0xc18e4
    rep movsw                                 ; f3 a5                       ; 0xc18e6
    pop DS                                    ; 1f                          ; 0xc18e8
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc18e9 vgabios.c:996
    xor ah, ah                                ; 30 e4                       ; 0xc18ec
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc18ee
    jc short 0191eh                           ; 72 2b                       ; 0xc18f1
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc18f3 vgabios.c:997
    jmp near 0183fh                           ; e9 46 ff                    ; 0xc18f6
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc18f9 vgabios.c:1004
    xor ah, ah                                ; 30 e4                       ; 0xc18fd
    mov bx, ax                                ; 89 c3                       ; 0xc18ff
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1901
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc1904
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1908
    mov bl, byte [di+04636h]                  ; 8a 9d 36 46                 ; 0xc190b vgabios.c:1005
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc190f
    je short 01921h                           ; 74 0d                       ; 0xc1912
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1914
    je short 01921h                           ; 74 08                       ; 0xc1917
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1919
    je short 01950h                           ; 74 32                       ; 0xc191c
    jmp near 01bbbh                           ; e9 9a 02                    ; 0xc191e
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1921 vgabios.c:1009
    jne short 0198bh                          ; 75 64                       ; 0xc1925
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1927
    jne short 0198bh                          ; 75 5e                       ; 0xc192b
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc192d
    jne short 0198bh                          ; 75 58                       ; 0xc1931
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1933
    xor ah, ah                                ; 30 e4                       ; 0xc1936
    mov dx, ax                                ; 89 c2                       ; 0xc1938
    mov ax, cx                                ; 89 c8                       ; 0xc193a
    dec ax                                    ; 48                          ; 0xc193c
    cmp dx, ax                                ; 39 c2                       ; 0xc193d
    jne short 0198bh                          ; 75 4a                       ; 0xc193f
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1941
    xor ah, ah                                ; 30 e4                       ; 0xc1944
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1946
    dec dx                                    ; 4a                          ; 0xc1949
    cmp ax, dx                                ; 39 d0                       ; 0xc194a
    je short 01953h                           ; 74 05                       ; 0xc194c
    jmp short 0198bh                          ; eb 3b                       ; 0xc194e
    jmp near 01a80h                           ; e9 2d 01                    ; 0xc1950
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1953 vgabios.c:1011
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1956
    out DX, ax                                ; ef                          ; 0xc1959
    mov ax, cx                                ; 89 c8                       ; 0xc195a vgabios.c:1012
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc195c
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc195f
    xor bh, bh                                ; 30 ff                       ; 0xc1962
    mul bx                                    ; f7 e3                       ; 0xc1964
    mov cx, ax                                ; 89 c1                       ; 0xc1966
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1968
    xor ah, ah                                ; 30 e4                       ; 0xc196b
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc196d
    xor dh, dh                                ; 30 f6                       ; 0xc1970
    mov bx, dx                                ; 89 d3                       ; 0xc1972
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1974
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1977
    xor di, di                                ; 31 ff                       ; 0xc197b
    cld                                       ; fc                          ; 0xc197d
    jcxz 01982h                               ; e3 02                       ; 0xc197e
    rep stosb                                 ; f3 aa                       ; 0xc1980
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1982 vgabios.c:1013
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1985
    out DX, ax                                ; ef                          ; 0xc1988
    jmp short 0191eh                          ; eb 93                       ; 0xc1989 vgabios.c:1015
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc198b vgabios.c:1017
    jne short 019e1h                          ; 75 50                       ; 0xc198f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1991 vgabios.c:1018
    xor ah, ah                                ; 30 e4                       ; 0xc1994
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1996
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1999
    xor ah, ah                                ; 30 e4                       ; 0xc199c
    mov dx, ax                                ; 89 c2                       ; 0xc199e
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc19a0
    jc short 01a07h                           ; 72 62                       ; 0xc19a3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc19a5 vgabios.c:1020
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc19a8
    cmp ax, dx                                ; 39 d0                       ; 0xc19ab
    jnbe short 019b5h                         ; 77 06                       ; 0xc19ad
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc19af
    jne short 019e3h                          ; 75 2e                       ; 0xc19b3
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc19b5 vgabios.c:1021
    xor ah, ah                                ; 30 e4                       ; 0xc19b8
    push ax                                   ; 50                          ; 0xc19ba
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc19bb
    push ax                                   ; 50                          ; 0xc19be
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc19bf
    mov cx, ax                                ; 89 c1                       ; 0xc19c2
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc19c4
    mov bx, ax                                ; 89 c3                       ; 0xc19c7
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc19c9
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc19cc
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc19cf
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc19d2
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc19d5
    mov dx, ax                                ; 89 c2                       ; 0xc19d8
    mov ax, si                                ; 89 f0                       ; 0xc19da
    call 01484h                               ; e8 a5 fa                    ; 0xc19dc
    jmp short 01a02h                          ; eb 21                       ; 0xc19df vgabios.c:1022
    jmp short 01a0ah                          ; eb 27                       ; 0xc19e1
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc19e3 vgabios.c:1023
    xor ah, ah                                ; 30 e4                       ; 0xc19e6
    push ax                                   ; 50                          ; 0xc19e8
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc19e9
    push ax                                   ; 50                          ; 0xc19ec
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc19ed
    mov cx, ax                                ; 89 c1                       ; 0xc19f0
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc19f2
    mov bx, ax                                ; 89 c3                       ; 0xc19f5
    add al, byte [bp-006h]                    ; 02 46 fa                    ; 0xc19f7
    mov dx, ax                                ; 89 c2                       ; 0xc19fa
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19fc
    call 013f5h                               ; e8 f3 f9                    ; 0xc19ff
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1a02 vgabios.c:1024
    jmp short 01999h                          ; eb 92                       ; 0xc1a05
    jmp near 01bbbh                           ; e9 b1 01                    ; 0xc1a07
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a0a vgabios.c:1027
    xor ah, ah                                ; 30 e4                       ; 0xc1a0d
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1a0f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1a12
    xor ah, ah                                ; 30 e4                       ; 0xc1a15
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a17
    jnbe short 01a07h                         ; 77 eb                       ; 0xc1a1a
    mov dx, ax                                ; 89 c2                       ; 0xc1a1c vgabios.c:1029
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a1e
    add ax, dx                                ; 01 d0                       ; 0xc1a21
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a23
    jnbe short 01a2eh                         ; 77 06                       ; 0xc1a26
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1a28
    jne short 01a4fh                          ; 75 21                       ; 0xc1a2c
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1a2e vgabios.c:1030
    xor ah, ah                                ; 30 e4                       ; 0xc1a31
    push ax                                   ; 50                          ; 0xc1a33
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1a34
    push ax                                   ; 50                          ; 0xc1a37
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1a38
    mov cx, ax                                ; 89 c1                       ; 0xc1a3b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1a3d
    mov bx, ax                                ; 89 c3                       ; 0xc1a40
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a42
    mov dx, ax                                ; 89 c2                       ; 0xc1a45
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a47
    call 01484h                               ; e8 37 fa                    ; 0xc1a4a
    jmp short 01a71h                          ; eb 22                       ; 0xc1a4d vgabios.c:1031
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1a4f vgabios.c:1032
    xor ah, ah                                ; 30 e4                       ; 0xc1a52
    push ax                                   ; 50                          ; 0xc1a54
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1a55
    push ax                                   ; 50                          ; 0xc1a58
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1a59
    mov cx, ax                                ; 89 c1                       ; 0xc1a5c
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a5e
    sub al, byte [bp-006h]                    ; 2a 46 fa                    ; 0xc1a61
    mov bx, ax                                ; 89 c3                       ; 0xc1a64
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a66
    mov dx, ax                                ; 89 c2                       ; 0xc1a69
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a6b
    call 013f5h                               ; e8 84 f9                    ; 0xc1a6e
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a71 vgabios.c:1033
    xor ah, ah                                ; 30 e4                       ; 0xc1a74
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a76
    jc short 01ac5h                           ; 72 4a                       ; 0xc1a79
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1a7b vgabios.c:1034
    jmp short 01a12h                          ; eb 92                       ; 0xc1a7e
    mov bl, byte [di+04637h]                  ; 8a 9d 37 46                 ; 0xc1a80 vgabios.c:1039
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1a84 vgabios.c:1040
    jne short 01ac8h                          ; 75 3e                       ; 0xc1a88
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1a8a
    jne short 01ac8h                          ; 75 38                       ; 0xc1a8e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1a90
    jne short 01ac8h                          ; 75 32                       ; 0xc1a94
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a96
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1a99
    jne short 01ac8h                          ; 75 2a                       ; 0xc1a9c
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a9e
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1aa1
    jne short 01ac8h                          ; 75 22                       ; 0xc1aa4
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1aa6 vgabios.c:1042
    mov dx, ax                                ; 89 c2                       ; 0xc1aa9
    mov ax, si                                ; 89 f0                       ; 0xc1aab
    mul dx                                    ; f7 e2                       ; 0xc1aad
    xor bh, bh                                ; 30 ff                       ; 0xc1aaf
    mul bx                                    ; f7 e3                       ; 0xc1ab1
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1ab3
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1ab6
    mov cx, ax                                ; 89 c1                       ; 0xc1aba
    mov ax, bx                                ; 89 d8                       ; 0xc1abc
    xor di, di                                ; 31 ff                       ; 0xc1abe
    cld                                       ; fc                          ; 0xc1ac0
    jcxz 01ac5h                               ; e3 02                       ; 0xc1ac1
    rep stosb                                 ; f3 aa                       ; 0xc1ac3
    jmp near 01bbbh                           ; e9 f3 00                    ; 0xc1ac5 vgabios.c:1044
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1ac8 vgabios.c:1046
    jne short 01ad6h                          ; 75 09                       ; 0xc1acb
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc1acd vgabios.c:1048
    sal byte [bp-00eh], 1                     ; d0 66 f2                    ; 0xc1ad0 vgabios.c:1049
    sal word [bp-018h], 1                     ; d1 66 e8                    ; 0xc1ad3 vgabios.c:1050
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1ad6 vgabios.c:1053
    jne short 01b45h                          ; 75 69                       ; 0xc1ada
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1adc vgabios.c:1054
    xor ah, ah                                ; 30 e4                       ; 0xc1adf
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1ae1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ae4
    xor ah, ah                                ; 30 e4                       ; 0xc1ae7
    mov dx, ax                                ; 89 c2                       ; 0xc1ae9
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1aeb
    jc short 01ac5h                           ; 72 d5                       ; 0xc1aee
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1af0 vgabios.c:1056
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc1af3
    cmp ax, dx                                ; 39 d0                       ; 0xc1af6
    jnbe short 01b00h                         ; 77 06                       ; 0xc1af8
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1afa
    jne short 01b21h                          ; 75 21                       ; 0xc1afe
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b00 vgabios.c:1057
    xor ah, ah                                ; 30 e4                       ; 0xc1b03
    push ax                                   ; 50                          ; 0xc1b05
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b06
    push ax                                   ; 50                          ; 0xc1b09
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b0a
    mov cx, ax                                ; 89 c1                       ; 0xc1b0d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b0f
    mov bx, ax                                ; 89 c3                       ; 0xc1b12
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b14
    mov dx, ax                                ; 89 c2                       ; 0xc1b17
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b19
    call 015c2h                               ; e8 a3 fa                    ; 0xc1b1c
    jmp short 01b40h                          ; eb 1f                       ; 0xc1b1f vgabios.c:1058
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b21 vgabios.c:1059
    xor ah, ah                                ; 30 e4                       ; 0xc1b24
    push ax                                   ; 50                          ; 0xc1b26
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b27
    push ax                                   ; 50                          ; 0xc1b2a
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b2b
    mov cx, ax                                ; 89 c1                       ; 0xc1b2e
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b30
    mov bx, ax                                ; 89 c3                       ; 0xc1b33
    add al, byte [bp-006h]                    ; 02 46 fa                    ; 0xc1b35
    mov dx, ax                                ; 89 c2                       ; 0xc1b38
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b3a
    call 01500h                               ; e8 c0 f9                    ; 0xc1b3d
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1b40 vgabios.c:1060
    jmp short 01ae4h                          ; eb 9f                       ; 0xc1b43
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b45 vgabios.c:1063
    xor ah, ah                                ; 30 e4                       ; 0xc1b48
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1b4a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1b4d
    xor ah, ah                                ; 30 e4                       ; 0xc1b50
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1b52
    jnbe short 01bbbh                         ; 77 64                       ; 0xc1b55
    mov dx, ax                                ; 89 c2                       ; 0xc1b57 vgabios.c:1065
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b59
    add ax, dx                                ; 01 d0                       ; 0xc1b5c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1b5e
    jnbe short 01b69h                         ; 77 06                       ; 0xc1b61
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b63
    jne short 01b8ah                          ; 75 21                       ; 0xc1b67
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1b69 vgabios.c:1066
    xor ah, ah                                ; 30 e4                       ; 0xc1b6c
    push ax                                   ; 50                          ; 0xc1b6e
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b6f
    push ax                                   ; 50                          ; 0xc1b72
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b73
    mov cx, ax                                ; 89 c1                       ; 0xc1b76
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b78
    mov bx, ax                                ; 89 c3                       ; 0xc1b7b
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b7d
    mov dx, ax                                ; 89 c2                       ; 0xc1b80
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b82
    call 015c2h                               ; e8 3a fa                    ; 0xc1b85
    jmp short 01bach                          ; eb 22                       ; 0xc1b88 vgabios.c:1067
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b8a vgabios.c:1068
    xor ah, ah                                ; 30 e4                       ; 0xc1b8d
    push ax                                   ; 50                          ; 0xc1b8f
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b90
    push ax                                   ; 50                          ; 0xc1b93
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b94
    mov cx, ax                                ; 89 c1                       ; 0xc1b97
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b99
    sub al, byte [bp-006h]                    ; 2a 46 fa                    ; 0xc1b9c
    mov bx, ax                                ; 89 c3                       ; 0xc1b9f
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1ba1
    mov dx, ax                                ; 89 c2                       ; 0xc1ba4
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ba6
    call 01500h                               ; e8 54 f9                    ; 0xc1ba9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1bac vgabios.c:1069
    xor ah, ah                                ; 30 e4                       ; 0xc1baf
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1bb1
    jc short 01bbbh                           ; 72 05                       ; 0xc1bb4
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1bb6 vgabios.c:1070
    jmp short 01b4dh                          ; eb 92                       ; 0xc1bb9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1bbb vgabios.c:1081
    pop di                                    ; 5f                          ; 0xc1bbe
    pop si                                    ; 5e                          ; 0xc1bbf
    pop bp                                    ; 5d                          ; 0xc1bc0
    retn 00008h                               ; c2 08 00                    ; 0xc1bc1
  ; disGetNextSymbol 0xc1bc4 LB 0x2129 -> off=0x0 cb=00000000000000fb uValue=00000000000c1bc4 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc1bc4 LB 0xfb
    push bp                                   ; 55                          ; 0xc1bc4 vgabios.c:1084
    mov bp, sp                                ; 89 e5                       ; 0xc1bc5
    push si                                   ; 56                          ; 0xc1bc7
    push di                                   ; 57                          ; 0xc1bc8
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1bc9
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1bcc
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1bcf
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1bd2
    mov al, cl                                ; 88 c8                       ; 0xc1bd5
    cmp byte [bp+006h], 010h                  ; 80 7e 06 10                 ; 0xc1bd7 vgabios.c:1091
    je short 01be8h                           ; 74 0b                       ; 0xc1bdb
    cmp byte [bp+006h], 00eh                  ; 80 7e 06 0e                 ; 0xc1bdd
    jne short 01bedh                          ; 75 0a                       ; 0xc1be1
    mov di, 05bf2h                            ; bf f2 5b                    ; 0xc1be3 vgabios.c:1093
    jmp short 01bf0h                          ; eb 08                       ; 0xc1be6 vgabios.c:1094
    mov di, 069f2h                            ; bf f2 69                    ; 0xc1be8 vgabios.c:1096
    jmp short 01bf0h                          ; eb 03                       ; 0xc1beb vgabios.c:1097
    mov di, 053f2h                            ; bf f2 53                    ; 0xc1bed vgabios.c:1099
    xor ah, ah                                ; 30 e4                       ; 0xc1bf0 vgabios.c:1101
    mov bx, ax                                ; 89 c3                       ; 0xc1bf2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1bf4
    mov si, ax                                ; 89 c6                       ; 0xc1bf7
    mov ax, bx                                ; 89 d8                       ; 0xc1bf9
    imul si                                   ; f7 ee                       ; 0xc1bfb
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1bfd
    imul bx                                   ; f7 eb                       ; 0xc1c00
    mov bx, ax                                ; 89 c3                       ; 0xc1c02
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1c04
    xor ah, ah                                ; 30 e4                       ; 0xc1c07
    add ax, bx                                ; 01 d8                       ; 0xc1c09
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1c0b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c0e vgabios.c:1102
    xor ah, ah                                ; 30 e4                       ; 0xc1c11
    imul si                                   ; f7 ee                       ; 0xc1c13
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1c15
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc1c18 vgabios.c:1103
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1c1b
    out DX, ax                                ; ef                          ; 0xc1c1e
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1c1f vgabios.c:1104
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1c22
    out DX, ax                                ; ef                          ; 0xc1c25
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1c26 vgabios.c:1105
    je short 01c32h                           ; 74 06                       ; 0xc1c2a
    mov ax, 01803h                            ; b8 03 18                    ; 0xc1c2c vgabios.c:1107
    out DX, ax                                ; ef                          ; 0xc1c2f
    jmp short 01c36h                          ; eb 04                       ; 0xc1c30 vgabios.c:1109
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1c32 vgabios.c:1111
    out DX, ax                                ; ef                          ; 0xc1c35
    xor ch, ch                                ; 30 ed                       ; 0xc1c36 vgabios.c:1113
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc1c38
    jnc short 01ca7h                          ; 73 6a                       ; 0xc1c3b
    mov al, ch                                ; 88 e8                       ; 0xc1c3d vgabios.c:1115
    xor ah, ah                                ; 30 e4                       ; 0xc1c3f
    mov bx, ax                                ; 89 c3                       ; 0xc1c41
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c43
    mov si, ax                                ; 89 c6                       ; 0xc1c46
    mov ax, bx                                ; 89 d8                       ; 0xc1c48
    imul si                                   ; f7 ee                       ; 0xc1c4a
    mov si, word [bp-010h]                    ; 8b 76 f0                    ; 0xc1c4c
    add si, ax                                ; 01 c6                       ; 0xc1c4f
    mov byte [bp-00ch], bh                    ; 88 7e f4                    ; 0xc1c51 vgabios.c:1116
    jmp short 01c69h                          ; eb 13                       ; 0xc1c54
    xor bx, bx                                ; 31 db                       ; 0xc1c56 vgabios.c:1127
    mov dx, si                                ; 89 f2                       ; 0xc1c58
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1c5a
    call 03196h                               ; e8 36 15                    ; 0xc1c5d
    inc byte [bp-00ch]                        ; fe 46 f4                    ; 0xc1c60 vgabios.c:1129
    cmp byte [bp-00ch], 008h                  ; 80 7e f4 08                 ; 0xc1c63
    jnc short 01ca3h                          ; 73 3a                       ; 0xc1c67
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc1c69
    mov ax, 00080h                            ; b8 80 00                    ; 0xc1c6c
    sar ax, CL                                ; d3 f8                       ; 0xc1c6f
    xor ah, ah                                ; 30 e4                       ; 0xc1c71
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc1c73
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1c76
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc1c79
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1c7b
    out DX, ax                                ; ef                          ; 0xc1c7e
    mov dx, si                                ; 89 f2                       ; 0xc1c7f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1c81
    call 03188h                               ; e8 01 15                    ; 0xc1c84
    mov al, ch                                ; 88 e8                       ; 0xc1c87
    xor ah, ah                                ; 30 e4                       ; 0xc1c89
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc1c8b
    add bx, ax                                ; 01 c3                       ; 0xc1c8e
    add bx, di                                ; 01 fb                       ; 0xc1c90
    mov al, byte [bx]                         ; 8a 07                       ; 0xc1c92
    test word [bp-012h], ax                   ; 85 46 ee                    ; 0xc1c94
    je short 01c56h                           ; 74 bd                       ; 0xc1c97
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1c99
    and bl, 00fh                              ; 80 e3 0f                    ; 0xc1c9c
    xor bh, bh                                ; 30 ff                       ; 0xc1c9f
    jmp short 01c58h                          ; eb b5                       ; 0xc1ca1
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1ca3 vgabios.c:1130
    jmp short 01c38h                          ; eb 91                       ; 0xc1ca5
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc1ca7 vgabios.c:1131
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1caa
    out DX, ax                                ; ef                          ; 0xc1cad
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1cae vgabios.c:1132
    out DX, ax                                ; ef                          ; 0xc1cb1
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1cb2 vgabios.c:1133
    out DX, ax                                ; ef                          ; 0xc1cb5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1cb6 vgabios.c:1134
    pop di                                    ; 5f                          ; 0xc1cb9
    pop si                                    ; 5e                          ; 0xc1cba
    pop bp                                    ; 5d                          ; 0xc1cbb
    retn 00004h                               ; c2 04 00                    ; 0xc1cbc
  ; disGetNextSymbol 0xc1cbf LB 0x202e -> off=0x0 cb=0000000000000138 uValue=00000000000c1cbf 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc1cbf LB 0x138
    push bp                                   ; 55                          ; 0xc1cbf vgabios.c:1137
    mov bp, sp                                ; 89 e5                       ; 0xc1cc0
    push si                                   ; 56                          ; 0xc1cc2
    push di                                   ; 57                          ; 0xc1cc3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1cc4
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1cc7
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1cca
    mov al, bl                                ; 88 d8                       ; 0xc1ccd
    mov si, 053f2h                            ; be f2 53                    ; 0xc1ccf vgabios.c:1144
    xor ah, ah                                ; 30 e4                       ; 0xc1cd2 vgabios.c:1145
    mov bx, ax                                ; 89 c3                       ; 0xc1cd4
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1cd6
    mov dx, ax                                ; 89 c2                       ; 0xc1cd9
    mov ax, bx                                ; 89 d8                       ; 0xc1cdb
    imul dx                                   ; f7 ea                       ; 0xc1cdd
    mov bx, ax                                ; 89 c3                       ; 0xc1cdf
    mov al, cl                                ; 88 c8                       ; 0xc1ce1
    xor ah, ah                                ; 30 e4                       ; 0xc1ce3
    imul ax, ax, 00140h                       ; 69 c0 40 01                 ; 0xc1ce5
    add bx, ax                                ; 01 c3                       ; 0xc1ce9
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc1ceb
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1cee vgabios.c:1146
    xor ah, ah                                ; 30 e4                       ; 0xc1cf1
    mov di, ax                                ; 89 c7                       ; 0xc1cf3
    sal di, 003h                              ; c1 e7 03                    ; 0xc1cf5
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1cf8 vgabios.c:1147
    jmp near 01d50h                           ; e9 52 00                    ; 0xc1cfb
    xor al, al                                ; 30 c0                       ; 0xc1cfe vgabios.c:1160
    xor ah, ah                                ; 30 e4                       ; 0xc1d00 vgabios.c:1162
    jmp short 01d0fh                          ; eb 0b                       ; 0xc1d02
    or al, bl                                 ; 08 d8                       ; 0xc1d04 vgabios.c:1172
    shr ch, 1                                 ; d0 ed                       ; 0xc1d06 vgabios.c:1175
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1d08 vgabios.c:1176
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc1d0a
    jnc short 01d3ah                          ; 73 2b                       ; 0xc1d0d
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1d0f
    xor bh, bh                                ; 30 ff                       ; 0xc1d12
    add bx, di                                ; 01 fb                       ; 0xc1d14
    add bx, si                                ; 01 f3                       ; 0xc1d16
    mov bl, byte [bx]                         ; 8a 1f                       ; 0xc1d18
    xor bh, bh                                ; 30 ff                       ; 0xc1d1a
    mov dx, bx                                ; 89 da                       ; 0xc1d1c
    mov bl, ch                                ; 88 eb                       ; 0xc1d1e
    test dx, bx                               ; 85 da                       ; 0xc1d20
    je short 01d06h                           ; 74 e2                       ; 0xc1d22
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc1d24
    sub cl, ah                                ; 28 e1                       ; 0xc1d26
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1d28
    and bl, 001h                              ; 80 e3 01                    ; 0xc1d2b
    sal bl, CL                                ; d2 e3                       ; 0xc1d2e
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1d30
    je short 01d04h                           ; 74 ce                       ; 0xc1d34
    xor al, bl                                ; 30 d8                       ; 0xc1d36
    jmp short 01d06h                          ; eb cc                       ; 0xc1d38
    xor ah, ah                                ; 30 e4                       ; 0xc1d3a vgabios.c:1177
    mov bx, ax                                ; 89 c3                       ; 0xc1d3c
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1d3e
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1d41
    call 03196h                               ; e8 4f 14                    ; 0xc1d44
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1d47 vgabios.c:1179
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc1d4a
    jnc short 01da0h                          ; 73 50                       ; 0xc1d4e
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1d50
    xor ah, ah                                ; 30 e4                       ; 0xc1d53
    sar ax, 1                                 ; d1 f8                       ; 0xc1d55
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc1d57
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1d5a
    add dx, ax                                ; 01 c2                       ; 0xc1d5d
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc1d5f
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc1d62
    je short 01d6ch                           ; 74 04                       ; 0xc1d66
    add byte [bp-00bh], 020h                  ; 80 46 f5 20                 ; 0xc1d68
    mov CH, strict byte 080h                  ; b5 80                       ; 0xc1d6c
    cmp byte [bp+006h], 001h                  ; 80 7e 06 01                 ; 0xc1d6e
    jne short 01d85h                          ; 75 11                       ; 0xc1d72
    test byte [bp-008h], ch                   ; 84 6e f8                    ; 0xc1d74
    je short 01cfeh                           ; 74 85                       ; 0xc1d77
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1d79
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1d7c
    call 03188h                               ; e8 06 14                    ; 0xc1d7f
    jmp near 01d00h                           ; e9 7b ff                    ; 0xc1d82
    test ch, ch                               ; 84 ed                       ; 0xc1d85 vgabios.c:1181
    jbe short 01d47h                          ; 76 be                       ; 0xc1d87
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1d89 vgabios.c:1183
    je short 01d9ah                           ; 74 0b                       ; 0xc1d8d
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1d8f vgabios.c:1185
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1d92
    call 03188h                               ; e8 f0 13                    ; 0xc1d95
    jmp short 01d9ch                          ; eb 02                       ; 0xc1d98 vgabios.c:1187
    xor al, al                                ; 30 c0                       ; 0xc1d9a vgabios.c:1189
    xor ah, ah                                ; 30 e4                       ; 0xc1d9c vgabios.c:1191
    jmp short 01da7h                          ; eb 07                       ; 0xc1d9e
    jmp short 01deeh                          ; eb 4c                       ; 0xc1da0
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc1da2
    jnc short 01ddch                          ; 73 35                       ; 0xc1da5
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1da7 vgabios.c:1193
    xor bh, bh                                ; 30 ff                       ; 0xc1daa
    add bx, di                                ; 01 fb                       ; 0xc1dac
    add bx, si                                ; 01 f3                       ; 0xc1dae
    mov bl, byte [bx]                         ; 8a 1f                       ; 0xc1db0
    xor bh, bh                                ; 30 ff                       ; 0xc1db2
    mov dx, bx                                ; 89 da                       ; 0xc1db4
    mov bl, ch                                ; 88 eb                       ; 0xc1db6
    test dx, bx                               ; 85 da                       ; 0xc1db8
    je short 01dd6h                           ; 74 1a                       ; 0xc1dba
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1dbc vgabios.c:1194
    sub cl, ah                                ; 28 e1                       ; 0xc1dbe
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1dc0
    and bl, 003h                              ; 80 e3 03                    ; 0xc1dc3
    add cl, cl                                ; 00 c9                       ; 0xc1dc6
    sal bl, CL                                ; d2 e3                       ; 0xc1dc8
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1dca vgabios.c:1195
    je short 01dd4h                           ; 74 04                       ; 0xc1dce
    xor al, bl                                ; 30 d8                       ; 0xc1dd0 vgabios.c:1197
    jmp short 01dd6h                          ; eb 02                       ; 0xc1dd2 vgabios.c:1199
    or al, bl                                 ; 08 d8                       ; 0xc1dd4 vgabios.c:1201
    shr ch, 1                                 ; d0 ed                       ; 0xc1dd6 vgabios.c:1204
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1dd8 vgabios.c:1205
    jmp short 01da2h                          ; eb c6                       ; 0xc1dda
    xor ah, ah                                ; 30 e4                       ; 0xc1ddc vgabios.c:1206
    mov bx, ax                                ; 89 c3                       ; 0xc1dde
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1de0
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1de3
    call 03196h                               ; e8 ad 13                    ; 0xc1de6
    inc word [bp-00ch]                        ; ff 46 f4                    ; 0xc1de9 vgabios.c:1207
    jmp short 01d85h                          ; eb 97                       ; 0xc1dec vgabios.c:1208
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1dee vgabios.c:1211
    pop di                                    ; 5f                          ; 0xc1df1
    pop si                                    ; 5e                          ; 0xc1df2
    pop bp                                    ; 5d                          ; 0xc1df3
    retn 00004h                               ; c2 04 00                    ; 0xc1df4
  ; disGetNextSymbol 0xc1df7 LB 0x1ef6 -> off=0x0 cb=00000000000000aa uValue=00000000000c1df7 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc1df7 LB 0xaa
    push bp                                   ; 55                          ; 0xc1df7 vgabios.c:1214
    mov bp, sp                                ; 89 e5                       ; 0xc1df8
    push si                                   ; 56                          ; 0xc1dfa
    push di                                   ; 57                          ; 0xc1dfb
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1dfc
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1dff
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc1e02
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1e05
    mov al, cl                                ; 88 c8                       ; 0xc1e08
    mov si, 053f2h                            ; be f2 53                    ; 0xc1e0a vgabios.c:1221
    xor ah, ah                                ; 30 e4                       ; 0xc1e0d vgabios.c:1222
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1e0f
    xor bh, bh                                ; 30 ff                       ; 0xc1e12
    imul bx                                   ; f7 eb                       ; 0xc1e14
    sal ax, 006h                              ; c1 e0 06                    ; 0xc1e16
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1e19
    mov dx, bx                                ; 89 da                       ; 0xc1e1c
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1e1e
    add dx, ax                                ; 01 c2                       ; 0xc1e21
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1e23
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e26 vgabios.c:1223
    mov di, bx                                ; 89 df                       ; 0xc1e29
    sal di, 003h                              ; c1 e7 03                    ; 0xc1e2b
    xor cl, cl                                ; 30 c9                       ; 0xc1e2e vgabios.c:1224
    jmp short 01e76h                          ; eb 44                       ; 0xc1e30
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc1e32 vgabios.c:1228
    jnc short 01e6fh                          ; 73 38                       ; 0xc1e35
    xor dl, dl                                ; 30 d2                       ; 0xc1e37 vgabios.c:1230
    mov al, cl                                ; 88 c8                       ; 0xc1e39 vgabios.c:1231
    xor ah, ah                                ; 30 e4                       ; 0xc1e3b
    add ax, di                                ; 01 f8                       ; 0xc1e3d
    mov bx, si                                ; 89 f3                       ; 0xc1e3f
    add bx, ax                                ; 01 c3                       ; 0xc1e41
    mov al, byte [bx]                         ; 8a 07                       ; 0xc1e43
    xor ah, ah                                ; 30 e4                       ; 0xc1e45
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1e47
    xor bh, bh                                ; 30 ff                       ; 0xc1e4a
    test ax, bx                               ; 85 d8                       ; 0xc1e4c
    je short 01e53h                           ; 74 03                       ; 0xc1e4e
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc1e50 vgabios.c:1233
    mov bl, dl                                ; 88 d3                       ; 0xc1e53 vgabios.c:1235
    xor bh, bh                                ; 30 ff                       ; 0xc1e55
    mov ax, bx                                ; 89 d8                       ; 0xc1e57
    mov bl, ch                                ; 88 eb                       ; 0xc1e59
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc1e5b
    add dx, bx                                ; 01 da                       ; 0xc1e5e
    mov bx, ax                                ; 89 c3                       ; 0xc1e60
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1e62
    call 03196h                               ; e8 2e 13                    ; 0xc1e65
    shr byte [bp-008h], 1                     ; d0 6e f8                    ; 0xc1e68 vgabios.c:1236
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1e6b vgabios.c:1237
    jmp short 01e32h                          ; eb c3                       ; 0xc1e6d
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc1e6f vgabios.c:1238
    cmp cl, 008h                              ; 80 f9 08                    ; 0xc1e71
    jnc short 01e98h                          ; 73 22                       ; 0xc1e74
    mov bl, cl                                ; 88 cb                       ; 0xc1e76
    xor bh, bh                                ; 30 ff                       ; 0xc1e78
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e7a
    xor ah, ah                                ; 30 e4                       ; 0xc1e7d
    mov dx, ax                                ; 89 c2                       ; 0xc1e7f
    mov ax, bx                                ; 89 d8                       ; 0xc1e81
    imul dx                                   ; f7 ea                       ; 0xc1e83
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1e85
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1e88
    add dx, ax                                ; 01 c2                       ; 0xc1e8b
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc1e8d
    mov byte [bp-008h], 080h                  ; c6 46 f8 80                 ; 0xc1e90
    xor ch, ch                                ; 30 ed                       ; 0xc1e94
    jmp short 01e37h                          ; eb 9f                       ; 0xc1e96
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1e98 vgabios.c:1239
    pop di                                    ; 5f                          ; 0xc1e9b
    pop si                                    ; 5e                          ; 0xc1e9c
    pop bp                                    ; 5d                          ; 0xc1e9d
    retn 00002h                               ; c2 02 00                    ; 0xc1e9e
  ; disGetNextSymbol 0xc1ea1 LB 0x1e4c -> off=0x0 cb=000000000000018d uValue=00000000000c1ea1 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc1ea1 LB 0x18d
    push bp                                   ; 55                          ; 0xc1ea1 vgabios.c:1242
    mov bp, sp                                ; 89 e5                       ; 0xc1ea2
    push si                                   ; 56                          ; 0xc1ea4
    push di                                   ; 57                          ; 0xc1ea5
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1ea6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1ea9
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc1eac
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1eaf
    mov si, cx                                ; 89 ce                       ; 0xc1eb2
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1eb4 vgabios.c:1249
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1eb7
    call 03188h                               ; e8 cb 12                    ; 0xc1eba
    xor ah, ah                                ; 30 e4                       ; 0xc1ebd vgabios.c:1250
    call 03160h                               ; e8 9e 12                    ; 0xc1ebf
    mov cl, al                                ; 88 c1                       ; 0xc1ec2
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1ec4
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1ec7 vgabios.c:1251
    jne short 01eceh                          ; 75 03                       ; 0xc1ec9
    jmp near 02027h                           ; e9 59 01                    ; 0xc1ecb
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1ece vgabios.c:1254
    xor ah, ah                                ; 30 e4                       ; 0xc1ed1
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc1ed3
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc1ed6
    call 00a8bh                               ; e8 af eb                    ; 0xc1ed9
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc1edc vgabios.c:1255
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1edf
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1ee2
    xor al, al                                ; 30 c0                       ; 0xc1ee5
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1ee7
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1eea
    mov dx, 00084h                            ; ba 84 00                    ; 0xc1eed vgabios.c:1258
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1ef0
    call 03188h                               ; e8 92 12                    ; 0xc1ef3
    xor ah, ah                                ; 30 e4                       ; 0xc1ef6
    inc ax                                    ; 40                          ; 0xc1ef8
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1ef9
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc1efc vgabios.c:1259
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1eff
    call 031a4h                               ; e8 9f 12                    ; 0xc1f02
    mov bx, ax                                ; 89 c3                       ; 0xc1f05
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f07
    mov al, cl                                ; 88 c8                       ; 0xc1f0a vgabios.c:1261
    xor ah, ah                                ; 30 e4                       ; 0xc1f0c
    mov di, ax                                ; 89 c7                       ; 0xc1f0e
    sal di, 003h                              ; c1 e7 03                    ; 0xc1f10
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1f13
    jne short 01f6ch                          ; 75 52                       ; 0xc1f18
    mov ax, bx                                ; 89 d8                       ; 0xc1f1a vgabios.c:1264
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1f1c
    add ax, ax                                ; 01 c0                       ; 0xc1f1f
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1f21
    mov dx, ax                                ; 89 c2                       ; 0xc1f23
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f25
    xor ah, ah                                ; 30 e4                       ; 0xc1f28
    mov cx, ax                                ; 89 c1                       ; 0xc1f2a
    mov ax, dx                                ; 89 d0                       ; 0xc1f2c
    inc ax                                    ; 40                          ; 0xc1f2e
    mul cx                                    ; f7 e1                       ; 0xc1f2f
    mov cx, ax                                ; 89 c1                       ; 0xc1f31
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1f33
    xor ah, ah                                ; 30 e4                       ; 0xc1f36
    mul bx                                    ; f7 e3                       ; 0xc1f38
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1f3a
    xor bh, bh                                ; 30 ff                       ; 0xc1f3d
    mov dx, ax                                ; 89 c2                       ; 0xc1f3f
    add dx, bx                                ; 01 da                       ; 0xc1f41
    add dx, dx                                ; 01 d2                       ; 0xc1f43
    add dx, cx                                ; 01 ca                       ; 0xc1f45
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f47 vgabios.c:1266
    xor ah, ah                                ; 30 e4                       ; 0xc1f4a
    mov bx, ax                                ; 89 c3                       ; 0xc1f4c
    sal bx, 008h                              ; c1 e3 08                    ; 0xc1f4e
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1f51
    add bx, ax                                ; 01 c3                       ; 0xc1f54
    mov word [bp-01ah], bx                    ; 89 5e e6                    ; 0xc1f56
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1f59 vgabios.c:1267
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1f5c
    mov cx, si                                ; 89 f1                       ; 0xc1f60
    mov di, dx                                ; 89 d7                       ; 0xc1f62
    cld                                       ; fc                          ; 0xc1f64
    jcxz 01f69h                               ; e3 02                       ; 0xc1f65
    rep stosw                                 ; f3 ab                       ; 0xc1f67
    jmp near 02027h                           ; e9 bb 00                    ; 0xc1f69 vgabios.c:1269
    mov bx, ax                                ; 89 c3                       ; 0xc1f6c vgabios.c:1272
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc1f6e
    mov bx, ax                                ; 89 c3                       ; 0xc1f72
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1f74
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc1f77
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1f7b
    mov al, byte [di+04637h]                  ; 8a 85 37 46                 ; 0xc1f7e vgabios.c:1273
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1f82
    dec si                                    ; 4e                          ; 0xc1f85 vgabios.c:1274
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc1f86
    je short 01f95h                           ; 74 0a                       ; 0xc1f89
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1f8b
    xor ah, ah                                ; 30 e4                       ; 0xc1f8e
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f90
    jc short 01f98h                           ; 72 03                       ; 0xc1f93
    jmp near 02027h                           ; e9 8f 00                    ; 0xc1f95
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc1f98 vgabios.c:1276
    mov bx, ax                                ; 89 c3                       ; 0xc1f9b
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1f9d
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc1fa0
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1fa4
    jc short 01fb4h                           ; 72 0c                       ; 0xc1fa6
    jbe short 01fbah                          ; 76 10                       ; 0xc1fa8
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1faa
    je short 02008h                           ; 74 5a                       ; 0xc1fac
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1fae
    je short 01fbah                           ; 74 08                       ; 0xc1fb0
    jmp short 02021h                          ; eb 6d                       ; 0xc1fb2
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1fb4
    je short 01fe3h                           ; 74 2b                       ; 0xc1fb6
    jmp short 02021h                          ; eb 67                       ; 0xc1fb8
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc1fba vgabios.c:1280
    xor bh, bh                                ; 30 ff                       ; 0xc1fbd
    push bx                                   ; 53                          ; 0xc1fbf
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1fc0
    push bx                                   ; 53                          ; 0xc1fc3
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1fc4
    mov cx, bx                                ; 89 d9                       ; 0xc1fc7
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1fc9
    xor ah, ah                                ; 30 e4                       ; 0xc1fcc
    mov dx, ax                                ; 89 c2                       ; 0xc1fce
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1fd0
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1fd3
    mov di, bx                                ; 89 df                       ; 0xc1fd6
    mov bx, dx                                ; 89 d3                       ; 0xc1fd8
    mov dx, ax                                ; 89 c2                       ; 0xc1fda
    mov ax, di                                ; 89 f8                       ; 0xc1fdc
    call 01bc4h                               ; e8 e3 fb                    ; 0xc1fde
    jmp short 02021h                          ; eb 3e                       ; 0xc1fe1 vgabios.c:1281
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1fe3 vgabios.c:1283
    push ax                                   ; 50                          ; 0xc1fe6
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1fe7
    push ax                                   ; 50                          ; 0xc1fea
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1feb
    mov cx, ax                                ; 89 c1                       ; 0xc1fee
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ff0
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1ff3
    xor bh, bh                                ; 30 ff                       ; 0xc1ff6
    mov dx, bx                                ; 89 da                       ; 0xc1ff8
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1ffa
    mov di, bx                                ; 89 df                       ; 0xc1ffd
    mov bx, ax                                ; 89 c3                       ; 0xc1fff
    mov ax, di                                ; 89 f8                       ; 0xc2001
    call 01cbfh                               ; e8 b9 fc                    ; 0xc2003
    jmp short 02021h                          ; eb 19                       ; 0xc2006 vgabios.c:1284
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2008 vgabios.c:1286
    push ax                                   ; 50                          ; 0xc200b
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc200c
    xor bh, bh                                ; 30 ff                       ; 0xc200f
    mov cx, bx                                ; 89 d9                       ; 0xc2011
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2013
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2016
    mov dx, ax                                ; 89 c2                       ; 0xc2019
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc201b
    call 01df7h                               ; e8 d6 fd                    ; 0xc201e
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2021 vgabios.c:1293
    jmp near 01f85h                           ; e9 5e ff                    ; 0xc2024 vgabios.c:1294
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2027 vgabios.c:1296
    pop di                                    ; 5f                          ; 0xc202a
    pop si                                    ; 5e                          ; 0xc202b
    pop bp                                    ; 5d                          ; 0xc202c
    retn                                      ; c3                          ; 0xc202d
  ; disGetNextSymbol 0xc202e LB 0x1cbf -> off=0x0 cb=0000000000000196 uValue=00000000000c202e 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc202e LB 0x196
    push bp                                   ; 55                          ; 0xc202e vgabios.c:1299
    mov bp, sp                                ; 89 e5                       ; 0xc202f
    push si                                   ; 56                          ; 0xc2031
    push di                                   ; 57                          ; 0xc2032
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2033
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2036
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2039
    mov byte [bp-010h], bl                    ; 88 5e f0                    ; 0xc203c
    mov si, cx                                ; 89 ce                       ; 0xc203f
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2041 vgabios.c:1306
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2044
    call 03188h                               ; e8 3e 11                    ; 0xc2047
    mov bl, al                                ; 88 c3                       ; 0xc204a vgabios.c:1307
    xor bh, bh                                ; 30 ff                       ; 0xc204c
    mov ax, bx                                ; 89 d8                       ; 0xc204e
    call 03160h                               ; e8 0d 11                    ; 0xc2050
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2053
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2056
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2059 vgabios.c:1308
    jne short 02060h                          ; 75 03                       ; 0xc205b
    jmp near 021bdh                           ; e9 5d 01                    ; 0xc205d
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2060 vgabios.c:1311
    mov ax, bx                                ; 89 d8                       ; 0xc2063
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc2065
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc2068
    call 00a8bh                               ; e8 1d ea                    ; 0xc206b
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc206e vgabios.c:1312
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2071
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2074
    xor al, al                                ; 30 c0                       ; 0xc2077
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2079
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc207c
    mov dx, 00084h                            ; ba 84 00                    ; 0xc207f vgabios.c:1315
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2082
    call 03188h                               ; e8 00 11                    ; 0xc2085
    mov bl, al                                ; 88 c3                       ; 0xc2088
    xor bh, bh                                ; 30 ff                       ; 0xc208a
    inc bx                                    ; 43                          ; 0xc208c
    mov word [bp-01ah], bx                    ; 89 5e e6                    ; 0xc208d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2090 vgabios.c:1316
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2093
    call 031a4h                               ; e8 0b 11                    ; 0xc2096
    mov cx, ax                                ; 89 c1                       ; 0xc2099
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc209b
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc209e vgabios.c:1318
    xor bh, bh                                ; 30 ff                       ; 0xc20a1
    mov di, bx                                ; 89 df                       ; 0xc20a3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc20a5
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc20a8
    jne short 020f6h                          ; 75 47                       ; 0xc20ad
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc20af vgabios.c:1321
    add ax, ax                                ; 01 c0                       ; 0xc20b2
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc20b4
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc20b6
    xor bh, bh                                ; 30 ff                       ; 0xc20b9
    inc ax                                    ; 40                          ; 0xc20bb
    mul bx                                    ; f7 e3                       ; 0xc20bc
    mov di, ax                                ; 89 c7                       ; 0xc20be
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc20c0
    mov ax, bx                                ; 89 d8                       ; 0xc20c3
    mul cx                                    ; f7 e1                       ; 0xc20c5
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc20c7
    add ax, bx                                ; 01 d8                       ; 0xc20ca
    add ax, ax                                ; 01 c0                       ; 0xc20cc
    mov cx, di                                ; 89 f9                       ; 0xc20ce
    add cx, ax                                ; 01 c1                       ; 0xc20d0
    dec si                                    ; 4e                          ; 0xc20d2 vgabios.c:1323
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc20d3
    je short 0205dh                           ; 74 85                       ; 0xc20d6
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc20d8 vgabios.c:1324
    xor ah, ah                                ; 30 e4                       ; 0xc20db
    mov dx, ax                                ; 89 c2                       ; 0xc20dd
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20df
    mov bx, ax                                ; 89 c3                       ; 0xc20e2
    sal bx, 003h                              ; c1 e3 03                    ; 0xc20e4
    mov ax, word [bx+04638h]                  ; 8b 87 38 46                 ; 0xc20e7
    mov bx, dx                                ; 89 d3                       ; 0xc20eb
    mov dx, cx                                ; 89 ca                       ; 0xc20ed
    call 03196h                               ; e8 a4 10                    ; 0xc20ef
    inc cx                                    ; 41                          ; 0xc20f2 vgabios.c:1325
    inc cx                                    ; 41                          ; 0xc20f3
    jmp short 020d2h                          ; eb dc                       ; 0xc20f4 vgabios.c:1326
    mov al, byte [di+046b4h]                  ; 8a 85 b4 46                 ; 0xc20f6 vgabios.c:1331
    xor ah, ah                                ; 30 e4                       ; 0xc20fa
    mov di, ax                                ; 89 c7                       ; 0xc20fc
    sal di, 006h                              ; c1 e7 06                    ; 0xc20fe
    mov al, byte [di+046cah]                  ; 8a 85 ca 46                 ; 0xc2101
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2105
    mov al, byte [bx+04637h]                  ; 8a 87 37 46                 ; 0xc2108 vgabios.c:1332
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc210c
    dec si                                    ; 4e                          ; 0xc210f vgabios.c:1333
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2110
    je short 0216dh                           ; 74 58                       ; 0xc2113
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc2115
    xor ah, ah                                ; 30 e4                       ; 0xc2118
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc211a
    jnc short 0216dh                          ; 73 4e                       ; 0xc211d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc211f vgabios.c:1335
    mov bx, ax                                ; 89 c3                       ; 0xc2122
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2124
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc2127
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc212b
    jc short 0213fh                           ; 72 0f                       ; 0xc212e
    jbe short 02146h                          ; 76 14                       ; 0xc2130
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2132
    je short 02196h                           ; 74 5f                       ; 0xc2135
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2137
    je short 02146h                           ; 74 0a                       ; 0xc213a
    jmp near 021b7h                           ; e9 78 00                    ; 0xc213c
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc213f
    je short 0216fh                           ; 74 2b                       ; 0xc2142
    jmp short 021b7h                          ; eb 71                       ; 0xc2144
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2146 vgabios.c:1339
    xor ah, ah                                ; 30 e4                       ; 0xc2149
    push ax                                   ; 50                          ; 0xc214b
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc214c
    push ax                                   ; 50                          ; 0xc214f
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc2150
    xor bh, bh                                ; 30 ff                       ; 0xc2153
    mov cx, bx                                ; 89 d9                       ; 0xc2155
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2157
    mov dx, bx                                ; 89 da                       ; 0xc215a
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc215c
    mov di, bx                                ; 89 df                       ; 0xc215f
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2161
    mov bx, dx                                ; 89 d3                       ; 0xc2164
    mov dx, di                                ; 89 fa                       ; 0xc2166
    call 01bc4h                               ; e8 59 fa                    ; 0xc2168
    jmp short 021b7h                          ; eb 4a                       ; 0xc216b vgabios.c:1340
    jmp short 021bdh                          ; eb 4e                       ; 0xc216d
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc216f vgabios.c:1342
    push ax                                   ; 50                          ; 0xc2172
    mov bl, byte [bp-018h]                    ; 8a 5e e8                    ; 0xc2173
    xor bh, bh                                ; 30 ff                       ; 0xc2176
    push bx                                   ; 53                          ; 0xc2178
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc2179
    mov cx, bx                                ; 89 d9                       ; 0xc217c
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc217e
    mov ax, bx                                ; 89 d8                       ; 0xc2181
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc2183
    xor dh, dh                                ; 30 f6                       ; 0xc2186
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2188
    mov di, bx                                ; 89 df                       ; 0xc218b
    mov bx, ax                                ; 89 c3                       ; 0xc218d
    mov ax, di                                ; 89 f8                       ; 0xc218f
    call 01cbfh                               ; e8 2b fb                    ; 0xc2191
    jmp short 021b7h                          ; eb 21                       ; 0xc2194 vgabios.c:1343
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2196 vgabios.c:1345
    push ax                                   ; 50                          ; 0xc2199
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc219a
    xor bh, bh                                ; 30 ff                       ; 0xc219d
    mov cx, bx                                ; 89 d9                       ; 0xc219f
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc21a1
    mov dx, bx                                ; 89 da                       ; 0xc21a4
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc21a6
    mov di, ax                                ; 89 c7                       ; 0xc21a9
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc21ab
    mov ax, bx                                ; 89 d8                       ; 0xc21ae
    mov bx, dx                                ; 89 d3                       ; 0xc21b0
    mov dx, di                                ; 89 fa                       ; 0xc21b2
    call 01df7h                               ; e8 40 fc                    ; 0xc21b4
    inc byte [bp-014h]                        ; fe 46 ec                    ; 0xc21b7 vgabios.c:1352
    jmp near 0210fh                           ; e9 52 ff                    ; 0xc21ba vgabios.c:1353
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc21bd vgabios.c:1355
    pop di                                    ; 5f                          ; 0xc21c0
    pop si                                    ; 5e                          ; 0xc21c1
    pop bp                                    ; 5d                          ; 0xc21c2
    retn                                      ; c3                          ; 0xc21c3
  ; disGetNextSymbol 0xc21c4 LB 0x1b29 -> off=0x0 cb=000000000000017b uValue=00000000000c21c4 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc21c4 LB 0x17b
    push bp                                   ; 55                          ; 0xc21c4 vgabios.c:1358
    mov bp, sp                                ; 89 e5                       ; 0xc21c5
    push si                                   ; 56                          ; 0xc21c7
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc21c8
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc21cb
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc21ce
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc21d1 vgabios.c:1364
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc21d4
    call 03188h                               ; e8 ae 0f                    ; 0xc21d7
    mov bl, al                                ; 88 c3                       ; 0xc21da vgabios.c:1365
    xor bh, bh                                ; 30 ff                       ; 0xc21dc
    mov ax, bx                                ; 89 d8                       ; 0xc21de
    call 03160h                               ; e8 7d 0f                    ; 0xc21e0
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc21e3
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc21e6 vgabios.c:1366
    je short 02210h                           ; 74 26                       ; 0xc21e8
    xor ah, ah                                ; 30 e4                       ; 0xc21ea vgabios.c:1367
    mov bx, ax                                ; 89 c3                       ; 0xc21ec
    sal bx, 003h                              ; c1 e3 03                    ; 0xc21ee
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc21f1
    je short 02210h                           ; 74 18                       ; 0xc21f6
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc21f8 vgabios.c:1369
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc21fc
    jc short 0220ch                           ; 72 0c                       ; 0xc21fe
    jbe short 02216h                          ; 76 14                       ; 0xc2200
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2202
    je short 02213h                           ; 74 0d                       ; 0xc2204
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2206
    je short 02216h                           ; 74 0c                       ; 0xc2208
    jmp short 02210h                          ; eb 04                       ; 0xc220a
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc220c
    je short 02283h                           ; 74 73                       ; 0xc220e
    jmp near 02312h                           ; e9 ff 00                    ; 0xc2210
    jmp near 02318h                           ; e9 02 01                    ; 0xc2213
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2216 vgabios.c:1373
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2219
    call 031a4h                               ; e8 85 0f                    ; 0xc221c
    mov bx, ax                                ; 89 c3                       ; 0xc221f
    mov ax, cx                                ; 89 c8                       ; 0xc2221
    mul bx                                    ; f7 e3                       ; 0xc2223
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc2225
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2228
    add bx, ax                                ; 01 c3                       ; 0xc222b
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc222d
    mov cx, word [bp-00ah]                    ; 8b 4e f6                    ; 0xc2230 vgabios.c:1374
    and cl, 007h                              ; 80 e1 07                    ; 0xc2233
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2236
    sar ax, CL                                ; d3 f8                       ; 0xc2239
    mov bl, al                                ; 88 c3                       ; 0xc223b vgabios.c:1375
    xor bh, bh                                ; 30 ff                       ; 0xc223d
    mov ax, bx                                ; 89 d8                       ; 0xc223f
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2241
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2244
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2246
    out DX, ax                                ; ef                          ; 0xc2249
    mov ax, 00205h                            ; b8 05 02                    ; 0xc224a vgabios.c:1376
    out DX, ax                                ; ef                          ; 0xc224d
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc224e vgabios.c:1377
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2251
    call 03188h                               ; e8 31 0f                    ; 0xc2254
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc2257 vgabios.c:1378
    je short 02264h                           ; 74 07                       ; 0xc225b
    mov ax, 01803h                            ; b8 03 18                    ; 0xc225d vgabios.c:1380
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2260
    out DX, ax                                ; ef                          ; 0xc2263
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2264 vgabios.c:1382
    xor bh, bh                                ; 30 ff                       ; 0xc2267
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc2269
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc226c
    call 03196h                               ; e8 24 0f                    ; 0xc226f
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2272 vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2275
    out DX, ax                                ; ef                          ; 0xc2278
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2279 vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc227c
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc227d vgabios.c:1385
    out DX, ax                                ; ef                          ; 0xc2280
    jmp short 02210h                          ; eb 8d                       ; 0xc2281 vgabios.c:1386
    mov ax, cx                                ; 89 c8                       ; 0xc2283 vgabios.c:1388
    shr ax, 1                                 ; d1 e8                       ; 0xc2285
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc2287
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc228a
    jne short 02299h                          ; 75 08                       ; 0xc228f
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc2291 vgabios.c:1390
    shr bx, 002h                              ; c1 eb 02                    ; 0xc2294
    jmp short 0229fh                          ; eb 06                       ; 0xc2297 vgabios.c:1392
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc2299 vgabios.c:1394
    shr bx, 003h                              ; c1 eb 03                    ; 0xc229c
    add bx, ax                                ; 01 c3                       ; 0xc229f
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc22a1
    test cl, 001h                             ; f6 c1 01                    ; 0xc22a4 vgabios.c:1396
    je short 022adh                           ; 74 04                       ; 0xc22a7
    add byte [bp-007h], 020h                  ; 80 46 f9 20                 ; 0xc22a9
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc22ad vgabios.c:1397
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22b0
    call 03188h                               ; e8 d2 0e                    ; 0xc22b3
    mov bl, al                                ; 88 c3                       ; 0xc22b6
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc22b8 vgabios.c:1398
    xor ah, ah                                ; 30 e4                       ; 0xc22bb
    mov si, ax                                ; 89 c6                       ; 0xc22bd
    sal si, 003h                              ; c1 e6 03                    ; 0xc22bf
    cmp byte [si+04637h], 002h                ; 80 bc 37 46 02              ; 0xc22c2
    jne short 022e2h                          ; 75 19                       ; 0xc22c7
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc22c9 vgabios.c:1400
    and AL, strict byte 003h                  ; 24 03                       ; 0xc22cc
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc22ce
    sub ah, al                                ; 28 c4                       ; 0xc22d0
    mov cl, ah                                ; 88 e1                       ; 0xc22d2
    add cl, ah                                ; 00 e1                       ; 0xc22d4
    mov bh, byte [bp-006h]                    ; 8a 7e fa                    ; 0xc22d6
    and bh, 003h                              ; 80 e7 03                    ; 0xc22d9
    sal bh, CL                                ; d2 e7                       ; 0xc22dc
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc22de vgabios.c:1401
    jmp short 022f5h                          ; eb 13                       ; 0xc22e0 vgabios.c:1403
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc22e2 vgabios.c:1405
    and AL, strict byte 007h                  ; 24 07                       ; 0xc22e5
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc22e7
    sub cl, al                                ; 28 c1                       ; 0xc22e9
    mov bh, byte [bp-006h]                    ; 8a 7e fa                    ; 0xc22eb
    and bh, 001h                              ; 80 e7 01                    ; 0xc22ee
    sal bh, CL                                ; d2 e7                       ; 0xc22f1
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc22f3 vgabios.c:1406
    sal al, CL                                ; d2 e0                       ; 0xc22f5
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc22f7 vgabios.c:1408
    je short 02301h                           ; 74 04                       ; 0xc22fb
    xor bl, bh                                ; 30 fb                       ; 0xc22fd vgabios.c:1410
    jmp short 02307h                          ; eb 06                       ; 0xc22ff vgabios.c:1412
    not al                                    ; f6 d0                       ; 0xc2301 vgabios.c:1414
    and bl, al                                ; 20 c3                       ; 0xc2303
    or bl, bh                                 ; 08 fb                       ; 0xc2305 vgabios.c:1415
    xor bh, bh                                ; 30 ff                       ; 0xc2307 vgabios.c:1417
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc2309
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc230c
    call 03196h                               ; e8 84 0e                    ; 0xc230f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2312 vgabios.c:1418
    pop si                                    ; 5e                          ; 0xc2315
    pop bp                                    ; 5d                          ; 0xc2316
    retn                                      ; c3                          ; 0xc2317
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2318 vgabios.c:1420
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc231b
    call 031a4h                               ; e8 83 0e                    ; 0xc231e
    mov bx, ax                                ; 89 c3                       ; 0xc2321
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2323
    mov ax, cx                                ; 89 c8                       ; 0xc2326
    mul bx                                    ; f7 e3                       ; 0xc2328
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc232a
    add bx, ax                                ; 01 c3                       ; 0xc232d
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc232f
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2332 vgabios.c:1421
    xor bh, bh                                ; 30 ff                       ; 0xc2335
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc2337
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc233a
    jmp short 0230fh                          ; eb d0                       ; 0xc233d
  ; disGetNextSymbol 0xc233f LB 0x19ae -> off=0x0 cb=000000000000026f uValue=00000000000c233f 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc233f LB 0x26f
    push bp                                   ; 55                          ; 0xc233f vgabios.c:1431
    mov bp, sp                                ; 89 e5                       ; 0xc2340
    push si                                   ; 56                          ; 0xc2342
    push di                                   ; 57                          ; 0xc2343
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2344
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2347
    mov byte [bp-016h], dl                    ; 88 56 ea                    ; 0xc234a
    mov byte [bp-010h], bl                    ; 88 5e f0                    ; 0xc234d
    mov byte [bp-014h], cl                    ; 88 4e ec                    ; 0xc2350
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2353 vgabios.c:1439
    jne short 02364h                          ; 75 0c                       ; 0xc2356
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2358 vgabios.c:1440
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc235b
    call 03188h                               ; e8 27 0e                    ; 0xc235e
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc2361
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2364 vgabios.c:1443
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2367
    call 03188h                               ; e8 1b 0e                    ; 0xc236a
    mov bl, al                                ; 88 c3                       ; 0xc236d vgabios.c:1444
    xor bh, bh                                ; 30 ff                       ; 0xc236f
    mov ax, bx                                ; 89 d8                       ; 0xc2371
    call 03160h                               ; e8 ea 0d                    ; 0xc2373
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2376
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2379 vgabios.c:1445
    je short 023e5h                           ; 74 68                       ; 0xc237b
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc237d vgabios.c:1448
    mov ax, bx                                ; 89 d8                       ; 0xc2380
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc2382
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc2385
    call 00a8bh                               ; e8 00 e7                    ; 0xc2388
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc238b vgabios.c:1449
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc238e
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2391
    xor al, al                                ; 30 c0                       ; 0xc2394
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2396
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2399
    mov dx, 00084h                            ; ba 84 00                    ; 0xc239c vgabios.c:1452
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc239f
    call 03188h                               ; e8 e3 0d                    ; 0xc23a2
    mov bl, al                                ; 88 c3                       ; 0xc23a5
    xor bh, bh                                ; 30 ff                       ; 0xc23a7
    inc bx                                    ; 43                          ; 0xc23a9
    mov word [bp-01ah], bx                    ; 89 5e e6                    ; 0xc23aa
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc23ad vgabios.c:1453
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23b0
    call 031a4h                               ; e8 ee 0d                    ; 0xc23b3
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc23b6
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc23b9 vgabios.c:1455
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc23bc
    jc short 023cch                           ; 72 0c                       ; 0xc23be
    jbe short 023d3h                          ; 76 11                       ; 0xc23c0
    cmp AL, strict byte 00dh                  ; 3c 0d                       ; 0xc23c2
    je short 023deh                           ; 74 18                       ; 0xc23c4
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc23c6
    je short 023e8h                           ; 74 1e                       ; 0xc23c8
    jmp short 023ebh                          ; eb 1f                       ; 0xc23ca
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc23cc
    jne short 023ebh                          ; 75 1b                       ; 0xc23ce
    jmp near 024f3h                           ; e9 20 01                    ; 0xc23d0
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc23d3 vgabios.c:1462
    jbe short 023e2h                          ; 76 09                       ; 0xc23d7
    dec byte [bp-00ch]                        ; fe 4e f4                    ; 0xc23d9
    jmp short 023e2h                          ; eb 04                       ; 0xc23dc vgabios.c:1463
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc23de vgabios.c:1470
    jmp near 024f3h                           ; e9 0e 01                    ; 0xc23e2 vgabios.c:1471
    jmp near 025a7h                           ; e9 bf 01                    ; 0xc23e5
    jmp near 024f0h                           ; e9 05 01                    ; 0xc23e8
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc23eb vgabios.c:1475
    xor bh, bh                                ; 30 ff                       ; 0xc23ee
    mov si, bx                                ; 89 de                       ; 0xc23f0
    sal si, 003h                              ; c1 e6 03                    ; 0xc23f2
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc23f5
    jne short 0244ah                          ; 75 4e                       ; 0xc23fa
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc23fc vgabios.c:1478
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc23ff
    add ax, ax                                ; 01 c0                       ; 0xc2402
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2404
    mov dx, ax                                ; 89 c2                       ; 0xc2406
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2408
    xor ah, ah                                ; 30 e4                       ; 0xc240b
    mov bx, ax                                ; 89 c3                       ; 0xc240d
    mov ax, dx                                ; 89 d0                       ; 0xc240f
    inc ax                                    ; 40                          ; 0xc2411
    mul bx                                    ; f7 e3                       ; 0xc2412
    mov cx, ax                                ; 89 c1                       ; 0xc2414
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2416
    mov ax, bx                                ; 89 d8                       ; 0xc2419
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc241b
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc241e
    add ax, bx                                ; 01 d8                       ; 0xc2421
    add ax, ax                                ; 01 c0                       ; 0xc2423
    add cx, ax                                ; 01 c1                       ; 0xc2425
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2427 vgabios.c:1481
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc242a
    mov dx, cx                                ; 89 ca                       ; 0xc242e
    call 03196h                               ; e8 63 0d                    ; 0xc2430
    cmp byte [bp-014h], 003h                  ; 80 7e ec 03                 ; 0xc2433 vgabios.c:1483
    jne short 02493h                          ; 75 5a                       ; 0xc2437
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc2439 vgabios.c:1484
    xor bh, bh                                ; 30 ff                       ; 0xc243c
    mov dx, cx                                ; 89 ca                       ; 0xc243e
    inc dx                                    ; 42                          ; 0xc2440
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc2441
    call 03196h                               ; e8 4e 0d                    ; 0xc2445
    jmp short 02493h                          ; eb 49                       ; 0xc2448 vgabios.c:1486
    mov bl, byte [bx+046b4h]                  ; 8a 9f b4 46                 ; 0xc244a vgabios.c:1489
    sal bx, 006h                              ; c1 e3 06                    ; 0xc244e
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc2451
    mov ah, byte [si+04637h]                  ; 8a a4 37 46                 ; 0xc2455 vgabios.c:1490
    mov cl, byte [si+04636h]                  ; 8a 8c 36 46                 ; 0xc2459 vgabios.c:1491
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc245d
    jc short 02470h                           ; 72 0e                       ; 0xc2460
    jbe short 02477h                          ; 76 13                       ; 0xc2462
    cmp cl, 005h                              ; 80 f9 05                    ; 0xc2464
    je short 024bdh                           ; 74 54                       ; 0xc2467
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc2469
    je short 02477h                           ; 74 09                       ; 0xc246c
    jmp short 024e0h                          ; eb 70                       ; 0xc246e
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc2470
    je short 02495h                           ; 74 20                       ; 0xc2473
    jmp short 024e0h                          ; eb 69                       ; 0xc2475
    xor ah, ah                                ; 30 e4                       ; 0xc2477 vgabios.c:1495
    push ax                                   ; 50                          ; 0xc2479
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc247a
    push ax                                   ; 50                          ; 0xc247d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc247e
    mov cx, ax                                ; 89 c1                       ; 0xc2481
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2483
    mov bx, ax                                ; 89 c3                       ; 0xc2486
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc2488
    xor dh, dh                                ; 30 f6                       ; 0xc248b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc248d
    call 01bc4h                               ; e8 31 f7                    ; 0xc2490
    jmp short 024e0h                          ; eb 4b                       ; 0xc2493 vgabios.c:1496
    mov al, ah                                ; 88 e0                       ; 0xc2495 vgabios.c:1498
    xor ah, ah                                ; 30 e4                       ; 0xc2497
    push ax                                   ; 50                          ; 0xc2499
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc249a
    push ax                                   ; 50                          ; 0xc249d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc249e
    mov cx, ax                                ; 89 c1                       ; 0xc24a1
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc24a3
    mov si, ax                                ; 89 c6                       ; 0xc24a6
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc24a8
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc24ab
    xor bh, bh                                ; 30 ff                       ; 0xc24ae
    mov di, bx                                ; 89 df                       ; 0xc24b0
    mov bx, si                                ; 89 f3                       ; 0xc24b2
    mov dx, ax                                ; 89 c2                       ; 0xc24b4
    mov ax, di                                ; 89 f8                       ; 0xc24b6
    call 01cbfh                               ; e8 04 f8                    ; 0xc24b8
    jmp short 024e0h                          ; eb 23                       ; 0xc24bb vgabios.c:1499
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc24bd vgabios.c:1501
    xor ah, ah                                ; 30 e4                       ; 0xc24c0
    push ax                                   ; 50                          ; 0xc24c2
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc24c3
    xor bh, bh                                ; 30 ff                       ; 0xc24c6
    mov cx, bx                                ; 89 d9                       ; 0xc24c8
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc24ca
    mov ax, bx                                ; 89 d8                       ; 0xc24cd
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc24cf
    mov dx, bx                                ; 89 da                       ; 0xc24d2
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc24d4
    mov si, bx                                ; 89 de                       ; 0xc24d7
    mov bx, ax                                ; 89 c3                       ; 0xc24d9
    mov ax, si                                ; 89 f0                       ; 0xc24db
    call 01df7h                               ; e8 17 f9                    ; 0xc24dd
    inc byte [bp-00ch]                        ; fe 46 f4                    ; 0xc24e0 vgabios.c:1509
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc24e3 vgabios.c:1511
    xor bh, bh                                ; 30 ff                       ; 0xc24e6
    cmp bx, word [bp-018h]                    ; 3b 5e e8                    ; 0xc24e8
    jne short 024f3h                          ; 75 06                       ; 0xc24eb
    mov byte [bp-00ch], bh                    ; 88 7e f4                    ; 0xc24ed vgabios.c:1512
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc24f0 vgabios.c:1513
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc24f3 vgabios.c:1518
    xor bh, bh                                ; 30 ff                       ; 0xc24f6
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc24f8
    cmp bx, ax                                ; 39 c3                       ; 0xc24fb
    jne short 0256ah                          ; 75 6b                       ; 0xc24fd
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc24ff vgabios.c:1520
    xor bh, ah                                ; 30 e7                       ; 0xc2502
    mov si, bx                                ; 89 de                       ; 0xc2504
    sal si, 003h                              ; c1 e6 03                    ; 0xc2506
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2509
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc250c
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc250e
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2511
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2514
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2516
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc2519
    jne short 0256ch                          ; 75 4c                       ; 0xc251e
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc2520 vgabios.c:1522
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc2523
    add ax, ax                                ; 01 c0                       ; 0xc2526
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2528
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc252a
    xor dh, dh                                ; 30 f6                       ; 0xc252d
    inc ax                                    ; 40                          ; 0xc252f
    mul dx                                    ; f7 e2                       ; 0xc2530
    mov cx, ax                                ; 89 c1                       ; 0xc2532
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2534
    xor bh, bh                                ; 30 ff                       ; 0xc2537
    lea ax, [bx-001h]                         ; 8d 47 ff                    ; 0xc2539
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc253c
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc253f
    add ax, bx                                ; 01 d8                       ; 0xc2542
    add ax, ax                                ; 01 c0                       ; 0xc2544
    mov dx, cx                                ; 89 ca                       ; 0xc2546
    add dx, ax                                ; 01 c2                       ; 0xc2548
    inc dx                                    ; 42                          ; 0xc254a
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc254b
    call 03188h                               ; e8 36 0c                    ; 0xc254f
    push strict byte 00001h                   ; 6a 01                       ; 0xc2552 vgabios.c:1524
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2554
    push bx                                   ; 53                          ; 0xc2557
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2558
    push bx                                   ; 53                          ; 0xc255b
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc255c
    push bx                                   ; 53                          ; 0xc255f
    mov bl, al                                ; 88 c3                       ; 0xc2560
    mov dx, bx                                ; 89 da                       ; 0xc2562
    xor cx, cx                                ; 31 c9                       ; 0xc2564
    xor bl, al                                ; 30 c3                       ; 0xc2566
    jmp short 02581h                          ; eb 17                       ; 0xc2568 vgabios.c:1526
    jmp short 0258ah                          ; eb 1e                       ; 0xc256a
    push strict byte 00001h                   ; 6a 01                       ; 0xc256c vgabios.c:1528
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc256e
    xor bh, bh                                ; 30 ff                       ; 0xc2571
    push bx                                   ; 53                          ; 0xc2573
    mov bl, al                                ; 88 c3                       ; 0xc2574
    push bx                                   ; 53                          ; 0xc2576
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc2577
    push bx                                   ; 53                          ; 0xc257a
    xor cx, cx                                ; 31 c9                       ; 0xc257b
    xor bl, bl                                ; 30 db                       ; 0xc257d
    xor dx, dx                                ; 31 d2                       ; 0xc257f
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2581
    call 0166ah                               ; e8 e3 f0                    ; 0xc2584
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2587 vgabios.c:1530
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc258a vgabios.c:1534
    xor bh, bh                                ; 30 ff                       ; 0xc258d
    mov word [bp-01eh], bx                    ; 89 5e e2                    ; 0xc258f
    sal word [bp-01eh], 008h                  ; c1 66 e2 08                 ; 0xc2592
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc2596
    add word [bp-01eh], bx                    ; 01 5e e2                    ; 0xc2599
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc259c vgabios.c:1535
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc259f
    mov ax, bx                                ; 89 d8                       ; 0xc25a2
    call 00e79h                               ; e8 d2 e8                    ; 0xc25a4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc25a7 vgabios.c:1536
    pop di                                    ; 5f                          ; 0xc25aa
    pop si                                    ; 5e                          ; 0xc25ab
    pop bp                                    ; 5d                          ; 0xc25ac
    retn                                      ; c3                          ; 0xc25ad
  ; disGetNextSymbol 0xc25ae LB 0x173f -> off=0x0 cb=000000000000002c uValue=00000000000c25ae 'get_font_access'
get_font_access:                             ; 0xc25ae LB 0x2c
    push bp                                   ; 55                          ; 0xc25ae vgabios.c:1539
    mov bp, sp                                ; 89 e5                       ; 0xc25af
    push dx                                   ; 52                          ; 0xc25b1
    mov ax, 00100h                            ; b8 00 01                    ; 0xc25b2 vgabios.c:1541
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc25b5
    out DX, ax                                ; ef                          ; 0xc25b8
    mov ax, 00402h                            ; b8 02 04                    ; 0xc25b9 vgabios.c:1542
    out DX, ax                                ; ef                          ; 0xc25bc
    mov ax, 00704h                            ; b8 04 07                    ; 0xc25bd vgabios.c:1543
    out DX, ax                                ; ef                          ; 0xc25c0
    mov ax, 00300h                            ; b8 00 03                    ; 0xc25c1 vgabios.c:1544
    out DX, ax                                ; ef                          ; 0xc25c4
    mov ax, 00204h                            ; b8 04 02                    ; 0xc25c5 vgabios.c:1545
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc25c8
    out DX, ax                                ; ef                          ; 0xc25cb
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc25cc vgabios.c:1546
    out DX, ax                                ; ef                          ; 0xc25cf
    mov ax, 00406h                            ; b8 06 04                    ; 0xc25d0 vgabios.c:1547
    out DX, ax                                ; ef                          ; 0xc25d3
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc25d4 vgabios.c:1548
    pop dx                                    ; 5a                          ; 0xc25d7
    pop bp                                    ; 5d                          ; 0xc25d8
    retn                                      ; c3                          ; 0xc25d9
  ; disGetNextSymbol 0xc25da LB 0x1713 -> off=0x0 cb=000000000000003c uValue=00000000000c25da 'release_font_access'
release_font_access:                         ; 0xc25da LB 0x3c
    push bp                                   ; 55                          ; 0xc25da vgabios.c:1550
    mov bp, sp                                ; 89 e5                       ; 0xc25db
    push dx                                   ; 52                          ; 0xc25dd
    mov ax, 00100h                            ; b8 00 01                    ; 0xc25de vgabios.c:1552
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc25e1
    out DX, ax                                ; ef                          ; 0xc25e4
    mov ax, 00302h                            ; b8 02 03                    ; 0xc25e5 vgabios.c:1553
    out DX, ax                                ; ef                          ; 0xc25e8
    mov ax, 00304h                            ; b8 04 03                    ; 0xc25e9 vgabios.c:1554
    out DX, ax                                ; ef                          ; 0xc25ec
    mov ax, 00300h                            ; b8 00 03                    ; 0xc25ed vgabios.c:1555
    out DX, ax                                ; ef                          ; 0xc25f0
    mov dx, 003cch                            ; ba cc 03                    ; 0xc25f1 vgabios.c:1556
    in AL, DX                                 ; ec                          ; 0xc25f4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc25f5
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc25f7
    sal ax, 002h                              ; c1 e0 02                    ; 0xc25fa
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc25fd
    sal ax, 008h                              ; c1 e0 08                    ; 0xc25ff
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2602
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2604
    out DX, ax                                ; ef                          ; 0xc2607
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2608 vgabios.c:1557
    out DX, ax                                ; ef                          ; 0xc260b
    mov ax, 01005h                            ; b8 05 10                    ; 0xc260c vgabios.c:1558
    out DX, ax                                ; ef                          ; 0xc260f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2610 vgabios.c:1559
    pop dx                                    ; 5a                          ; 0xc2613
    pop bp                                    ; 5d                          ; 0xc2614
    retn                                      ; c3                          ; 0xc2615
  ; disGetNextSymbol 0xc2616 LB 0x16d7 -> off=0x0 cb=00000000000000c2 uValue=00000000000c2616 'set_scan_lines'
set_scan_lines:                              ; 0xc2616 LB 0xc2
    push bp                                   ; 55                          ; 0xc2616 vgabios.c:1561
    mov bp, sp                                ; 89 e5                       ; 0xc2617
    push bx                                   ; 53                          ; 0xc2619
    push cx                                   ; 51                          ; 0xc261a
    push dx                                   ; 52                          ; 0xc261b
    push si                                   ; 56                          ; 0xc261c
    push di                                   ; 57                          ; 0xc261d
    mov bl, al                                ; 88 c3                       ; 0xc261e
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2620 vgabios.c:1566
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2623
    call 031a4h                               ; e8 7b 0b                    ; 0xc2626
    mov dx, ax                                ; 89 c2                       ; 0xc2629
    mov si, ax                                ; 89 c6                       ; 0xc262b
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc262d vgabios.c:1567
    out DX, AL                                ; ee                          ; 0xc262f
    inc dx                                    ; 42                          ; 0xc2630 vgabios.c:1568
    in AL, DX                                 ; ec                          ; 0xc2631
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2632
    mov ah, al                                ; 88 c4                       ; 0xc2634 vgabios.c:1569
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2636
    mov al, bl                                ; 88 d8                       ; 0xc2639
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc263b
    or al, ah                                 ; 08 e0                       ; 0xc263d
    out DX, AL                                ; ee                          ; 0xc263f vgabios.c:1570
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2640 vgabios.c:1571
    jne short 0264dh                          ; 75 08                       ; 0xc2643
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2645 vgabios.c:1573
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2648
    jmp short 0265ah                          ; eb 0d                       ; 0xc264b vgabios.c:1575
    mov dl, bl                                ; 88 da                       ; 0xc264d vgabios.c:1577
    sub dl, 003h                              ; 80 ea 03                    ; 0xc264f
    xor dh, dh                                ; 30 f6                       ; 0xc2652
    mov al, bl                                ; 88 d8                       ; 0xc2654
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2656
    xor ah, ah                                ; 30 e4                       ; 0xc2658
    call 00dcbh                               ; e8 6e e7                    ; 0xc265a
    mov cl, bl                                ; 88 d9                       ; 0xc265d vgabios.c:1579
    xor ch, ch                                ; 30 ed                       ; 0xc265f
    mov bx, cx                                ; 89 cb                       ; 0xc2661
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2663
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2666
    call 031b2h                               ; e8 46 0b                    ; 0xc2669
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc266c vgabios.c:1580
    mov dx, si                                ; 89 f2                       ; 0xc266e
    out DX, AL                                ; ee                          ; 0xc2670
    lea bx, [si+001h]                         ; 8d 5c 01                    ; 0xc2671 vgabios.c:1581
    mov dx, bx                                ; 89 da                       ; 0xc2674
    in AL, DX                                 ; ec                          ; 0xc2676
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2677
    mov di, ax                                ; 89 c7                       ; 0xc2679
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc267b vgabios.c:1582
    mov dx, si                                ; 89 f2                       ; 0xc267d
    out DX, AL                                ; ee                          ; 0xc267f
    mov dx, bx                                ; 89 da                       ; 0xc2680 vgabios.c:1583
    in AL, DX                                 ; ec                          ; 0xc2682
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2683
    mov dl, al                                ; 88 c2                       ; 0xc2685 vgabios.c:1584
    and dl, 002h                              ; 80 e2 02                    ; 0xc2687
    xor dh, bh                                ; 30 fe                       ; 0xc268a
    sal dx, 007h                              ; c1 e2 07                    ; 0xc268c
    and AL, strict byte 040h                  ; 24 40                       ; 0xc268f
    xor ah, ah                                ; 30 e4                       ; 0xc2691
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2693
    add ax, dx                                ; 01 d0                       ; 0xc2696
    inc ax                                    ; 40                          ; 0xc2698
    add ax, di                                ; 01 f8                       ; 0xc2699
    xor dx, dx                                ; 31 d2                       ; 0xc269b vgabios.c:1585
    div cx                                    ; f7 f1                       ; 0xc269d
    mov cx, ax                                ; 89 c1                       ; 0xc269f
    mov bl, al                                ; 88 c3                       ; 0xc26a1 vgabios.c:1586
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc26a3
    xor bh, bh                                ; 30 ff                       ; 0xc26a5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc26a7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26aa
    call 03196h                               ; e8 e6 0a                    ; 0xc26ad
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc26b0 vgabios.c:1587
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26b3
    call 031a4h                               ; e8 eb 0a                    ; 0xc26b6
    mov dx, ax                                ; 89 c2                       ; 0xc26b9
    mov al, cl                                ; 88 c8                       ; 0xc26bb vgabios.c:1588
    xor ah, ah                                ; 30 e4                       ; 0xc26bd
    mul dx                                    ; f7 e2                       ; 0xc26bf
    mov bx, ax                                ; 89 c3                       ; 0xc26c1
    add bx, ax                                ; 01 c3                       ; 0xc26c3
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc26c5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26c8
    call 031b2h                               ; e8 e4 0a                    ; 0xc26cb
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc26ce vgabios.c:1589
    pop di                                    ; 5f                          ; 0xc26d1
    pop si                                    ; 5e                          ; 0xc26d2
    pop dx                                    ; 5a                          ; 0xc26d3
    pop cx                                    ; 59                          ; 0xc26d4
    pop bx                                    ; 5b                          ; 0xc26d5
    pop bp                                    ; 5d                          ; 0xc26d6
    retn                                      ; c3                          ; 0xc26d7
  ; disGetNextSymbol 0xc26d8 LB 0x1615 -> off=0x0 cb=0000000000000080 uValue=00000000000c26d8 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc26d8 LB 0x80
    push bp                                   ; 55                          ; 0xc26d8 vgabios.c:1591
    mov bp, sp                                ; 89 e5                       ; 0xc26d9
    push si                                   ; 56                          ; 0xc26db
    push di                                   ; 57                          ; 0xc26dc
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc26dd
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc26e0
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc26e3
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc26e6
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc26e9
    call 025aeh                               ; e8 bf fe                    ; 0xc26ec vgabios.c:1596
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc26ef vgabios.c:1597
    and AL, strict byte 003h                  ; 24 03                       ; 0xc26f2
    xor ah, ah                                ; 30 e4                       ; 0xc26f4
    mov bx, ax                                ; 89 c3                       ; 0xc26f6
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc26f8
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc26fb
    and AL, strict byte 004h                  ; 24 04                       ; 0xc26fe
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2700
    add bx, ax                                ; 01 c3                       ; 0xc2703
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2705
    xor bx, bx                                ; 31 db                       ; 0xc2708 vgabios.c:1598
    cmp bx, word [bp-00eh]                    ; 3b 5e f2                    ; 0xc270a
    jnc short 0273eh                          ; 73 2f                       ; 0xc270d
    mov cl, byte [bp+008h]                    ; 8a 4e 08                    ; 0xc270f vgabios.c:1600
    xor ch, ch                                ; 30 ed                       ; 0xc2712
    mov ax, bx                                ; 89 d8                       ; 0xc2714
    mul cx                                    ; f7 e1                       ; 0xc2716
    mov si, word [bp-00ah]                    ; 8b 76 f6                    ; 0xc2718
    add si, ax                                ; 01 c6                       ; 0xc271b
    mov ax, word [bp+004h]                    ; 8b 46 04                    ; 0xc271d vgabios.c:1601
    add ax, bx                                ; 01 d8                       ; 0xc2720
    sal ax, 005h                              ; c1 e0 05                    ; 0xc2722
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc2725
    add di, ax                                ; 01 c7                       ; 0xc2728
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc272a vgabios.c:1602
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc272d
    mov es, ax                                ; 8e c0                       ; 0xc2730
    cld                                       ; fc                          ; 0xc2732
    jcxz 0273bh                               ; e3 06                       ; 0xc2733
    push DS                                   ; 1e                          ; 0xc2735
    mov ds, dx                                ; 8e da                       ; 0xc2736
    rep movsb                                 ; f3 a4                       ; 0xc2738
    pop DS                                    ; 1f                          ; 0xc273a
    inc bx                                    ; 43                          ; 0xc273b vgabios.c:1603
    jmp short 0270ah                          ; eb cc                       ; 0xc273c
    call 025dah                               ; e8 99 fe                    ; 0xc273e vgabios.c:1604
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2741 vgabios.c:1605
    jc short 0274fh                           ; 72 08                       ; 0xc2745
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2747 vgabios.c:1607
    xor ah, ah                                ; 30 e4                       ; 0xc274a
    call 02616h                               ; e8 c7 fe                    ; 0xc274c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc274f vgabios.c:1609
    pop di                                    ; 5f                          ; 0xc2752
    pop si                                    ; 5e                          ; 0xc2753
    pop bp                                    ; 5d                          ; 0xc2754
    retn 00006h                               ; c2 06 00                    ; 0xc2755
  ; disGetNextSymbol 0xc2758 LB 0x1595 -> off=0x0 cb=000000000000006e uValue=00000000000c2758 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2758 LB 0x6e
    push bp                                   ; 55                          ; 0xc2758 vgabios.c:1611
    mov bp, sp                                ; 89 e5                       ; 0xc2759
    push bx                                   ; 53                          ; 0xc275b
    push cx                                   ; 51                          ; 0xc275c
    push si                                   ; 56                          ; 0xc275d
    push di                                   ; 57                          ; 0xc275e
    push ax                                   ; 50                          ; 0xc275f
    push ax                                   ; 50                          ; 0xc2760
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2761
    call 025aeh                               ; e8 47 fe                    ; 0xc2764 vgabios.c:1615
    mov al, dl                                ; 88 d0                       ; 0xc2767 vgabios.c:1616
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2769
    xor ah, ah                                ; 30 e4                       ; 0xc276b
    mov bx, ax                                ; 89 c3                       ; 0xc276d
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc276f
    mov al, dl                                ; 88 d0                       ; 0xc2772
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2774
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2776
    add bx, ax                                ; 01 c3                       ; 0xc2779
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc277b
    xor bx, bx                                ; 31 db                       ; 0xc277e vgabios.c:1617
    jmp short 02788h                          ; eb 06                       ; 0xc2780
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2782
    jnc short 027aeh                          ; 73 26                       ; 0xc2786
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2788 vgabios.c:1619
    mov di, bx                                ; 89 df                       ; 0xc278b vgabios.c:1620
    sal di, 005h                              ; c1 e7 05                    ; 0xc278d
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2790
    add si, 05bf2h                            ; 81 c6 f2 5b                 ; 0xc2793 vgabios.c:1621
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2797
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc279a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc279d
    mov es, ax                                ; 8e c0                       ; 0xc27a0
    cld                                       ; fc                          ; 0xc27a2
    jcxz 027abh                               ; e3 06                       ; 0xc27a3
    push DS                                   ; 1e                          ; 0xc27a5
    mov ds, dx                                ; 8e da                       ; 0xc27a6
    rep movsb                                 ; f3 a4                       ; 0xc27a8
    pop DS                                    ; 1f                          ; 0xc27aa
    inc bx                                    ; 43                          ; 0xc27ab vgabios.c:1622
    jmp short 02782h                          ; eb d4                       ; 0xc27ac
    call 025dah                               ; e8 29 fe                    ; 0xc27ae vgabios.c:1623
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc27b1 vgabios.c:1624
    jc short 027bdh                           ; 72 06                       ; 0xc27b5
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc27b7 vgabios.c:1626
    call 02616h                               ; e8 59 fe                    ; 0xc27ba
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc27bd vgabios.c:1628
    pop di                                    ; 5f                          ; 0xc27c0
    pop si                                    ; 5e                          ; 0xc27c1
    pop cx                                    ; 59                          ; 0xc27c2
    pop bx                                    ; 5b                          ; 0xc27c3
    pop bp                                    ; 5d                          ; 0xc27c4
    retn                                      ; c3                          ; 0xc27c5
  ; disGetNextSymbol 0xc27c6 LB 0x1527 -> off=0x0 cb=0000000000000070 uValue=00000000000c27c6 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc27c6 LB 0x70
    push bp                                   ; 55                          ; 0xc27c6 vgabios.c:1630
    mov bp, sp                                ; 89 e5                       ; 0xc27c7
    push bx                                   ; 53                          ; 0xc27c9
    push cx                                   ; 51                          ; 0xc27ca
    push si                                   ; 56                          ; 0xc27cb
    push di                                   ; 57                          ; 0xc27cc
    push ax                                   ; 50                          ; 0xc27cd
    push ax                                   ; 50                          ; 0xc27ce
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc27cf
    call 025aeh                               ; e8 d9 fd                    ; 0xc27d2 vgabios.c:1634
    mov al, dl                                ; 88 d0                       ; 0xc27d5 vgabios.c:1635
    and AL, strict byte 003h                  ; 24 03                       ; 0xc27d7
    xor ah, ah                                ; 30 e4                       ; 0xc27d9
    mov bx, ax                                ; 89 c3                       ; 0xc27db
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc27dd
    mov al, dl                                ; 88 d0                       ; 0xc27e0
    and AL, strict byte 004h                  ; 24 04                       ; 0xc27e2
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc27e4
    add bx, ax                                ; 01 c3                       ; 0xc27e7
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc27e9
    xor bx, bx                                ; 31 db                       ; 0xc27ec vgabios.c:1636
    jmp short 027f6h                          ; eb 06                       ; 0xc27ee
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc27f0
    jnc short 0281eh                          ; 73 28                       ; 0xc27f4
    mov si, bx                                ; 89 de                       ; 0xc27f6 vgabios.c:1638
    sal si, 003h                              ; c1 e6 03                    ; 0xc27f8
    mov di, bx                                ; 89 df                       ; 0xc27fb vgabios.c:1639
    sal di, 005h                              ; c1 e7 05                    ; 0xc27fd
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2800
    add si, 053f2h                            ; 81 c6 f2 53                 ; 0xc2803 vgabios.c:1640
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2807
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc280a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc280d
    mov es, ax                                ; 8e c0                       ; 0xc2810
    cld                                       ; fc                          ; 0xc2812
    jcxz 0281bh                               ; e3 06                       ; 0xc2813
    push DS                                   ; 1e                          ; 0xc2815
    mov ds, dx                                ; 8e da                       ; 0xc2816
    rep movsb                                 ; f3 a4                       ; 0xc2818
    pop DS                                    ; 1f                          ; 0xc281a
    inc bx                                    ; 43                          ; 0xc281b vgabios.c:1641
    jmp short 027f0h                          ; eb d2                       ; 0xc281c
    call 025dah                               ; e8 b9 fd                    ; 0xc281e vgabios.c:1642
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2821 vgabios.c:1643
    jc short 0282dh                           ; 72 06                       ; 0xc2825
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2827 vgabios.c:1645
    call 02616h                               ; e8 e9 fd                    ; 0xc282a
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc282d vgabios.c:1647
    pop di                                    ; 5f                          ; 0xc2830
    pop si                                    ; 5e                          ; 0xc2831
    pop cx                                    ; 59                          ; 0xc2832
    pop bx                                    ; 5b                          ; 0xc2833
    pop bp                                    ; 5d                          ; 0xc2834
    retn                                      ; c3                          ; 0xc2835
  ; disGetNextSymbol 0xc2836 LB 0x14b7 -> off=0x0 cb=0000000000000070 uValue=00000000000c2836 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2836 LB 0x70
    push bp                                   ; 55                          ; 0xc2836 vgabios.c:1650
    mov bp, sp                                ; 89 e5                       ; 0xc2837
    push bx                                   ; 53                          ; 0xc2839
    push cx                                   ; 51                          ; 0xc283a
    push si                                   ; 56                          ; 0xc283b
    push di                                   ; 57                          ; 0xc283c
    push ax                                   ; 50                          ; 0xc283d
    push ax                                   ; 50                          ; 0xc283e
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc283f
    call 025aeh                               ; e8 69 fd                    ; 0xc2842 vgabios.c:1654
    mov al, dl                                ; 88 d0                       ; 0xc2845 vgabios.c:1655
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2847
    xor ah, ah                                ; 30 e4                       ; 0xc2849
    mov bx, ax                                ; 89 c3                       ; 0xc284b
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc284d
    mov al, dl                                ; 88 d0                       ; 0xc2850
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2852
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2854
    add bx, ax                                ; 01 c3                       ; 0xc2857
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2859
    xor bx, bx                                ; 31 db                       ; 0xc285c vgabios.c:1656
    jmp short 02866h                          ; eb 06                       ; 0xc285e
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2860
    jnc short 0288eh                          ; 73 28                       ; 0xc2864
    mov si, bx                                ; 89 de                       ; 0xc2866 vgabios.c:1658
    sal si, 004h                              ; c1 e6 04                    ; 0xc2868
    mov di, bx                                ; 89 df                       ; 0xc286b vgabios.c:1659
    sal di, 005h                              ; c1 e7 05                    ; 0xc286d
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2870
    add si, 069f2h                            ; 81 c6 f2 69                 ; 0xc2873 vgabios.c:1660
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2877
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc287a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc287d
    mov es, ax                                ; 8e c0                       ; 0xc2880
    cld                                       ; fc                          ; 0xc2882
    jcxz 0288bh                               ; e3 06                       ; 0xc2883
    push DS                                   ; 1e                          ; 0xc2885
    mov ds, dx                                ; 8e da                       ; 0xc2886
    rep movsb                                 ; f3 a4                       ; 0xc2888
    pop DS                                    ; 1f                          ; 0xc288a
    inc bx                                    ; 43                          ; 0xc288b vgabios.c:1661
    jmp short 02860h                          ; eb d2                       ; 0xc288c
    call 025dah                               ; e8 49 fd                    ; 0xc288e vgabios.c:1662
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2891 vgabios.c:1663
    jc short 0289dh                           ; 72 06                       ; 0xc2895
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2897 vgabios.c:1665
    call 02616h                               ; e8 79 fd                    ; 0xc289a
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc289d vgabios.c:1667
    pop di                                    ; 5f                          ; 0xc28a0
    pop si                                    ; 5e                          ; 0xc28a1
    pop cx                                    ; 59                          ; 0xc28a2
    pop bx                                    ; 5b                          ; 0xc28a3
    pop bp                                    ; 5d                          ; 0xc28a4
    retn                                      ; c3                          ; 0xc28a5
  ; disGetNextSymbol 0xc28a6 LB 0x1447 -> off=0x0 cb=0000000000000005 uValue=00000000000c28a6 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc28a6 LB 0x5
    push bp                                   ; 55                          ; 0xc28a6 vgabios.c:1669
    mov bp, sp                                ; 89 e5                       ; 0xc28a7
    pop bp                                    ; 5d                          ; 0xc28a9 vgabios.c:1674
    retn                                      ; c3                          ; 0xc28aa
  ; disGetNextSymbol 0xc28ab LB 0x1442 -> off=0x0 cb=0000000000000007 uValue=00000000000c28ab 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc28ab LB 0x7
    push bp                                   ; 55                          ; 0xc28ab vgabios.c:1675
    mov bp, sp                                ; 89 e5                       ; 0xc28ac
    pop bp                                    ; 5d                          ; 0xc28ae vgabios.c:1681
    retn 00002h                               ; c2 02 00                    ; 0xc28af
  ; disGetNextSymbol 0xc28b2 LB 0x143b -> off=0x0 cb=0000000000000005 uValue=00000000000c28b2 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc28b2 LB 0x5
    push bp                                   ; 55                          ; 0xc28b2 vgabios.c:1682
    mov bp, sp                                ; 89 e5                       ; 0xc28b3
    pop bp                                    ; 5d                          ; 0xc28b5 vgabios.c:1687
    retn                                      ; c3                          ; 0xc28b6
  ; disGetNextSymbol 0xc28b7 LB 0x1436 -> off=0x0 cb=0000000000000005 uValue=00000000000c28b7 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc28b7 LB 0x5
    push bp                                   ; 55                          ; 0xc28b7 vgabios.c:1688
    mov bp, sp                                ; 89 e5                       ; 0xc28b8
    pop bp                                    ; 5d                          ; 0xc28ba vgabios.c:1693
    retn                                      ; c3                          ; 0xc28bb
  ; disGetNextSymbol 0xc28bc LB 0x1431 -> off=0x0 cb=0000000000000005 uValue=00000000000c28bc 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc28bc LB 0x5
    push bp                                   ; 55                          ; 0xc28bc vgabios.c:1694
    mov bp, sp                                ; 89 e5                       ; 0xc28bd
    pop bp                                    ; 5d                          ; 0xc28bf vgabios.c:1699
    retn                                      ; c3                          ; 0xc28c0
  ; disGetNextSymbol 0xc28c1 LB 0x142c -> off=0x0 cb=0000000000000005 uValue=00000000000c28c1 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc28c1 LB 0x5
    push bp                                   ; 55                          ; 0xc28c1 vgabios.c:1701
    mov bp, sp                                ; 89 e5                       ; 0xc28c2
    pop bp                                    ; 5d                          ; 0xc28c4 vgabios.c:1706
    retn                                      ; c3                          ; 0xc28c5
  ; disGetNextSymbol 0xc28c6 LB 0x1427 -> off=0x0 cb=0000000000000005 uValue=00000000000c28c6 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc28c6 LB 0x5
    push bp                                   ; 55                          ; 0xc28c6 vgabios.c:1709
    mov bp, sp                                ; 89 e5                       ; 0xc28c7
    pop bp                                    ; 5d                          ; 0xc28c9 vgabios.c:1714
    retn                                      ; c3                          ; 0xc28ca
  ; disGetNextSymbol 0xc28cb LB 0x1422 -> off=0x0 cb=0000000000000005 uValue=00000000000c28cb 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc28cb LB 0x5
    push bp                                   ; 55                          ; 0xc28cb vgabios.c:1715
    mov bp, sp                                ; 89 e5                       ; 0xc28cc
    pop bp                                    ; 5d                          ; 0xc28ce vgabios.c:1720
    retn                                      ; c3                          ; 0xc28cf
  ; disGetNextSymbol 0xc28d0 LB 0x141d -> off=0x0 cb=00000000000000a2 uValue=00000000000c28d0 'biosfn_write_string'
biosfn_write_string:                         ; 0xc28d0 LB 0xa2
    push bp                                   ; 55                          ; 0xc28d0 vgabios.c:1723
    mov bp, sp                                ; 89 e5                       ; 0xc28d1
    push si                                   ; 56                          ; 0xc28d3
    push di                                   ; 57                          ; 0xc28d4
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc28d5
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc28d8
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc28db
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc28de
    mov si, cx                                ; 89 ce                       ; 0xc28e1
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc28e3
    mov al, dl                                ; 88 d0                       ; 0xc28e6 vgabios.c:1730
    xor ah, ah                                ; 30 e4                       ; 0xc28e8
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc28ea
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc28ed
    call 00a8bh                               ; e8 98 e1                    ; 0xc28f0
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc28f3 vgabios.c:1733
    jne short 0290ah                          ; 75 11                       ; 0xc28f7
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc28f9 vgabios.c:1734
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc28fc
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc28ff vgabios.c:1735
    xor al, al                                ; 30 c0                       ; 0xc2902
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2904
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2907
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc290a vgabios.c:1738
    xor dh, dh                                ; 30 f6                       ; 0xc290d
    sal dx, 008h                              ; c1 e2 08                    ; 0xc290f
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2912
    xor ah, ah                                ; 30 e4                       ; 0xc2915
    add dx, ax                                ; 01 c2                       ; 0xc2917
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2919 vgabios.c:1739
    call 00e79h                               ; e8 5a e5                    ; 0xc291c
    dec si                                    ; 4e                          ; 0xc291f vgabios.c:1741
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2920
    je short 02958h                           ; 74 33                       ; 0xc2923
    mov dx, di                                ; 89 fa                       ; 0xc2925 vgabios.c:1743
    inc di                                    ; 47                          ; 0xc2927
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc2928
    call 03188h                               ; e8 5a 08                    ; 0xc292b
    mov cl, al                                ; 88 c1                       ; 0xc292e
    test byte [bp-006h], 002h                 ; f6 46 fa 02                 ; 0xc2930 vgabios.c:1744
    je short 02942h                           ; 74 0c                       ; 0xc2934
    mov dx, di                                ; 89 fa                       ; 0xc2936 vgabios.c:1745
    inc di                                    ; 47                          ; 0xc2938
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc2939
    call 03188h                               ; e8 49 08                    ; 0xc293c
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc293f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2942 vgabios.c:1747
    xor ah, ah                                ; 30 e4                       ; 0xc2945
    mov bx, ax                                ; 89 c3                       ; 0xc2947
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2949
    mov dx, ax                                ; 89 c2                       ; 0xc294c
    mov al, cl                                ; 88 c8                       ; 0xc294e
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2950
    call 0233fh                               ; e8 e9 f9                    ; 0xc2953
    jmp short 0291fh                          ; eb c7                       ; 0xc2956 vgabios.c:1748
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc2958 vgabios.c:1751
    jne short 02969h                          ; 75 0b                       ; 0xc295c
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc295e vgabios.c:1752
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2961
    xor ah, ah                                ; 30 e4                       ; 0xc2964
    call 00e79h                               ; e8 10 e5                    ; 0xc2966
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2969 vgabios.c:1753
    pop di                                    ; 5f                          ; 0xc296c
    pop si                                    ; 5e                          ; 0xc296d
    pop bp                                    ; 5d                          ; 0xc296e
    retn 00008h                               ; c2 08 00                    ; 0xc296f
  ; disGetNextSymbol 0xc2972 LB 0x137b -> off=0x0 cb=0000000000000102 uValue=00000000000c2972 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2972 LB 0x102
    push bp                                   ; 55                          ; 0xc2972 vgabios.c:1756
    mov bp, sp                                ; 89 e5                       ; 0xc2973
    push cx                                   ; 51                          ; 0xc2975
    push si                                   ; 56                          ; 0xc2976
    push di                                   ; 57                          ; 0xc2977
    push dx                                   ; 52                          ; 0xc2978
    push bx                                   ; 53                          ; 0xc2979
    mov cx, ds                                ; 8c d9                       ; 0xc297a vgabios.c:1759
    mov bx, 05388h                            ; bb 88 53                    ; 0xc297c
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc297f
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2982
    call 031d2h                               ; e8 4a 08                    ; 0xc2985
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2988 vgabios.c:1762
    add di, strict byte 00004h                ; 83 c7 04                    ; 0xc298b
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc298e
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2991
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2994
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc2997
    cld                                       ; fc                          ; 0xc299a
    jcxz 029a3h                               ; e3 06                       ; 0xc299b
    push DS                                   ; 1e                          ; 0xc299d
    mov ds, dx                                ; 8e da                       ; 0xc299e
    rep movsb                                 ; f3 a4                       ; 0xc29a0
    pop DS                                    ; 1f                          ; 0xc29a2
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc29a3 vgabios.c:1763
    add di, strict byte 00022h                ; 83 c7 22                    ; 0xc29a6
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc29a9
    mov si, 00084h                            ; be 84 00                    ; 0xc29ac
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc29af
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc29b2
    cld                                       ; fc                          ; 0xc29b5
    jcxz 029beh                               ; e3 06                       ; 0xc29b6
    push DS                                   ; 1e                          ; 0xc29b8
    mov ds, dx                                ; 8e da                       ; 0xc29b9
    rep movsb                                 ; f3 a4                       ; 0xc29bb
    pop DS                                    ; 1f                          ; 0xc29bd
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc29be vgabios.c:1765
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc29c1
    call 03188h                               ; e8 c1 07                    ; 0xc29c4
    mov bl, al                                ; 88 c3                       ; 0xc29c7
    xor bh, bh                                ; 30 ff                       ; 0xc29c9
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29cb
    add dx, strict byte 00025h                ; 83 c2 25                    ; 0xc29ce
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc29d1
    call 03196h                               ; e8 bf 07                    ; 0xc29d4
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29d7 vgabios.c:1766
    add dx, strict byte 00026h                ; 83 c2 26                    ; 0xc29da
    xor bx, bx                                ; 31 db                       ; 0xc29dd
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc29df
    call 03196h                               ; e8 b1 07                    ; 0xc29e2
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29e5 vgabios.c:1767
    add dx, strict byte 00027h                ; 83 c2 27                    ; 0xc29e8
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc29eb
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc29ee
    call 03196h                               ; e8 a2 07                    ; 0xc29f1
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29f4 vgabios.c:1768
    add dx, strict byte 00028h                ; 83 c2 28                    ; 0xc29f7
    xor bx, bx                                ; 31 db                       ; 0xc29fa
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc29fc
    call 03196h                               ; e8 94 07                    ; 0xc29ff
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a02 vgabios.c:1769
    add dx, strict byte 00029h                ; 83 c2 29                    ; 0xc2a05
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc2a08
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a0b
    call 03196h                               ; e8 85 07                    ; 0xc2a0e
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a11 vgabios.c:1770
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc2a14
    mov bx, strict word 00002h                ; bb 02 00                    ; 0xc2a17
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a1a
    call 03196h                               ; e8 76 07                    ; 0xc2a1d
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a20 vgabios.c:1771
    add dx, strict byte 0002bh                ; 83 c2 2b                    ; 0xc2a23
    xor bx, bx                                ; 31 db                       ; 0xc2a26
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a28
    call 03196h                               ; e8 68 07                    ; 0xc2a2b
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a2e vgabios.c:1772
    add dx, strict byte 0002ch                ; 83 c2 2c                    ; 0xc2a31
    xor bx, bx                                ; 31 db                       ; 0xc2a34
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a36
    call 03196h                               ; e8 5a 07                    ; 0xc2a39
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a3c vgabios.c:1773
    add dx, strict byte 00031h                ; 83 c2 31                    ; 0xc2a3f
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc2a42
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a45
    call 03196h                               ; e8 4b 07                    ; 0xc2a48
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a4b vgabios.c:1774
    add dx, strict byte 00032h                ; 83 c2 32                    ; 0xc2a4e
    xor bx, bx                                ; 31 db                       ; 0xc2a51
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a53
    call 03196h                               ; e8 3d 07                    ; 0xc2a56
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2a59 vgabios.c:1776
    add di, strict byte 00033h                ; 83 c7 33                    ; 0xc2a5c
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2a5f
    xor ax, ax                                ; 31 c0                       ; 0xc2a62
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc2a64
    cld                                       ; fc                          ; 0xc2a67
    jcxz 02a6ch                               ; e3 02                       ; 0xc2a68
    rep stosb                                 ; f3 aa                       ; 0xc2a6a
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2a6c vgabios.c:1777
    pop di                                    ; 5f                          ; 0xc2a6f
    pop si                                    ; 5e                          ; 0xc2a70
    pop cx                                    ; 59                          ; 0xc2a71
    pop bp                                    ; 5d                          ; 0xc2a72
    retn                                      ; c3                          ; 0xc2a73
  ; disGetNextSymbol 0xc2a74 LB 0x1279 -> off=0x0 cb=0000000000000023 uValue=00000000000c2a74 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc2a74 LB 0x23
    push dx                                   ; 52                          ; 0xc2a74 vgabios.c:1780
    push bp                                   ; 55                          ; 0xc2a75
    mov bp, sp                                ; 89 e5                       ; 0xc2a76
    mov dx, ax                                ; 89 c2                       ; 0xc2a78
    xor ax, ax                                ; 31 c0                       ; 0xc2a7a vgabios.c:1784
    test dl, 001h                             ; f6 c2 01                    ; 0xc2a7c vgabios.c:1785
    je short 02a84h                           ; 74 03                       ; 0xc2a7f
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2a81 vgabios.c:1786
    test dl, 002h                             ; f6 c2 02                    ; 0xc2a84 vgabios.c:1788
    je short 02a8ch                           ; 74 03                       ; 0xc2a87
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2a89 vgabios.c:1789
    test dl, 004h                             ; f6 c2 04                    ; 0xc2a8c vgabios.c:1791
    je short 02a94h                           ; 74 03                       ; 0xc2a8f
    add ax, 00304h                            ; 05 04 03                    ; 0xc2a91 vgabios.c:1792
    pop bp                                    ; 5d                          ; 0xc2a94 vgabios.c:1796
    pop dx                                    ; 5a                          ; 0xc2a95
    retn                                      ; c3                          ; 0xc2a96
  ; disGetNextSymbol 0xc2a97 LB 0x1256 -> off=0x0 cb=0000000000000012 uValue=00000000000c2a97 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc2a97 LB 0x12
    push bp                                   ; 55                          ; 0xc2a97 vgabios.c:1798
    mov bp, sp                                ; 89 e5                       ; 0xc2a98
    push bx                                   ; 53                          ; 0xc2a9a
    mov bx, dx                                ; 89 d3                       ; 0xc2a9b
    call 02a74h                               ; e8 d4 ff                    ; 0xc2a9d vgabios.c:1800
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc2aa0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2aa3 vgabios.c:1801
    pop bx                                    ; 5b                          ; 0xc2aa6
    pop bp                                    ; 5d                          ; 0xc2aa7
    retn                                      ; c3                          ; 0xc2aa8
  ; disGetNextSymbol 0xc2aa9 LB 0x1244 -> off=0x0 cb=0000000000000381 uValue=00000000000c2aa9 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc2aa9 LB 0x381
    push bp                                   ; 55                          ; 0xc2aa9 vgabios.c:1803
    mov bp, sp                                ; 89 e5                       ; 0xc2aaa
    push cx                                   ; 51                          ; 0xc2aac
    push si                                   ; 56                          ; 0xc2aad
    push di                                   ; 57                          ; 0xc2aae
    push ax                                   ; 50                          ; 0xc2aaf
    push ax                                   ; 50                          ; 0xc2ab0
    push ax                                   ; 50                          ; 0xc2ab1
    mov si, dx                                ; 89 d6                       ; 0xc2ab2
    mov cx, bx                                ; 89 d9                       ; 0xc2ab4
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2ab6 vgabios.c:1807
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ab9
    call 031a4h                               ; e8 e5 06                    ; 0xc2abc
    mov di, ax                                ; 89 c7                       ; 0xc2abf
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc2ac1 vgabios.c:1808
    je short 02b35h                           ; 74 6e                       ; 0xc2ac5
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2ac7 vgabios.c:1809
    in AL, DX                                 ; ec                          ; 0xc2aca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2acb
    xor ah, ah                                ; 30 e4                       ; 0xc2acd
    mov bx, ax                                ; 89 c3                       ; 0xc2acf
    mov dx, cx                                ; 89 ca                       ; 0xc2ad1
    mov ax, si                                ; 89 f0                       ; 0xc2ad3
    call 03196h                               ; e8 be 06                    ; 0xc2ad5
    inc cx                                    ; 41                          ; 0xc2ad8 vgabios.c:1810
    mov dx, di                                ; 89 fa                       ; 0xc2ad9
    in AL, DX                                 ; ec                          ; 0xc2adb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2adc
    xor ah, ah                                ; 30 e4                       ; 0xc2ade
    mov bx, ax                                ; 89 c3                       ; 0xc2ae0
    mov dx, cx                                ; 89 ca                       ; 0xc2ae2
    mov ax, si                                ; 89 f0                       ; 0xc2ae4
    call 03196h                               ; e8 ad 06                    ; 0xc2ae6
    inc cx                                    ; 41                          ; 0xc2ae9 vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2aea
    in AL, DX                                 ; ec                          ; 0xc2aed
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2aee
    xor ah, ah                                ; 30 e4                       ; 0xc2af0
    mov bx, ax                                ; 89 c3                       ; 0xc2af2
    mov dx, cx                                ; 89 ca                       ; 0xc2af4
    mov ax, si                                ; 89 f0                       ; 0xc2af6
    call 03196h                               ; e8 9b 06                    ; 0xc2af8
    inc cx                                    ; 41                          ; 0xc2afb vgabios.c:1812
    mov dx, 003dah                            ; ba da 03                    ; 0xc2afc
    in AL, DX                                 ; ec                          ; 0xc2aff
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b00
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2b02 vgabios.c:1813
    in AL, DX                                 ; ec                          ; 0xc2b05
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b06
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2b08
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2b0b vgabios.c:1814
    xor ah, ah                                ; 30 e4                       ; 0xc2b0e
    mov bx, ax                                ; 89 c3                       ; 0xc2b10
    mov dx, cx                                ; 89 ca                       ; 0xc2b12
    mov ax, si                                ; 89 f0                       ; 0xc2b14
    call 03196h                               ; e8 7d 06                    ; 0xc2b16
    inc cx                                    ; 41                          ; 0xc2b19 vgabios.c:1815
    mov dx, 003cah                            ; ba ca 03                    ; 0xc2b1a
    in AL, DX                                 ; ec                          ; 0xc2b1d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b1e
    xor ah, ah                                ; 30 e4                       ; 0xc2b20
    mov bx, ax                                ; 89 c3                       ; 0xc2b22
    mov dx, cx                                ; 89 ca                       ; 0xc2b24
    mov ax, si                                ; 89 f0                       ; 0xc2b26
    call 03196h                               ; e8 6b 06                    ; 0xc2b28
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2b2b vgabios.c:1817
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2b2e
    add cx, ax                                ; 01 c1                       ; 0xc2b31
    jmp short 02b3eh                          ; eb 09                       ; 0xc2b33
    jmp near 02c39h                           ; e9 01 01                    ; 0xc2b35
    cmp word [bp-00ah], strict byte 00004h    ; 83 7e f6 04                 ; 0xc2b38
    jnbe short 02b5ch                         ; 77 1e                       ; 0xc2b3c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2b3e vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b41
    out DX, AL                                ; ee                          ; 0xc2b44
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2b45 vgabios.c:1819
    in AL, DX                                 ; ec                          ; 0xc2b48
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b49
    xor ah, ah                                ; 30 e4                       ; 0xc2b4b
    mov bx, ax                                ; 89 c3                       ; 0xc2b4d
    mov dx, cx                                ; 89 ca                       ; 0xc2b4f
    mov ax, si                                ; 89 f0                       ; 0xc2b51
    call 03196h                               ; e8 40 06                    ; 0xc2b53
    inc cx                                    ; 41                          ; 0xc2b56
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2b57 vgabios.c:1820
    jmp short 02b38h                          ; eb dc                       ; 0xc2b5a
    xor al, al                                ; 30 c0                       ; 0xc2b5c vgabios.c:1821
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b5e
    out DX, AL                                ; ee                          ; 0xc2b61
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2b62 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc2b65
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b66
    xor ah, ah                                ; 30 e4                       ; 0xc2b68
    mov bx, ax                                ; 89 c3                       ; 0xc2b6a
    mov dx, cx                                ; 89 ca                       ; 0xc2b6c
    mov ax, si                                ; 89 f0                       ; 0xc2b6e
    call 03196h                               ; e8 23 06                    ; 0xc2b70
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2b73 vgabios.c:1824
    inc cx                                    ; 41                          ; 0xc2b78
    jmp short 02b81h                          ; eb 06                       ; 0xc2b79
    cmp word [bp-00ah], strict byte 00018h    ; 83 7e f6 18                 ; 0xc2b7b
    jnbe short 02b9eh                         ; 77 1d                       ; 0xc2b7f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2b81 vgabios.c:1825
    mov dx, di                                ; 89 fa                       ; 0xc2b84
    out DX, AL                                ; ee                          ; 0xc2b86
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2b87 vgabios.c:1826
    in AL, DX                                 ; ec                          ; 0xc2b8a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b8b
    xor ah, ah                                ; 30 e4                       ; 0xc2b8d
    mov bx, ax                                ; 89 c3                       ; 0xc2b8f
    mov dx, cx                                ; 89 ca                       ; 0xc2b91
    mov ax, si                                ; 89 f0                       ; 0xc2b93
    call 03196h                               ; e8 fe 05                    ; 0xc2b95
    inc cx                                    ; 41                          ; 0xc2b98
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2b99 vgabios.c:1827
    jmp short 02b7bh                          ; eb dd                       ; 0xc2b9c
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2b9e vgabios.c:1829
    jmp short 02babh                          ; eb 06                       ; 0xc2ba3
    cmp word [bp-00ah], strict byte 00013h    ; 83 7e f6 13                 ; 0xc2ba5
    jnbe short 02bd5h                         ; 77 2a                       ; 0xc2ba9
    mov dx, 003dah                            ; ba da 03                    ; 0xc2bab vgabios.c:1830
    in AL, DX                                 ; ec                          ; 0xc2bae
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2baf
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2bb1 vgabios.c:1831
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2bb4
    or ax, word [bp-00ah]                     ; 0b 46 f6                    ; 0xc2bb7
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2bba
    out DX, AL                                ; ee                          ; 0xc2bbd
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc2bbe vgabios.c:1832
    in AL, DX                                 ; ec                          ; 0xc2bc1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bc2
    xor ah, ah                                ; 30 e4                       ; 0xc2bc4
    mov bx, ax                                ; 89 c3                       ; 0xc2bc6
    mov dx, cx                                ; 89 ca                       ; 0xc2bc8
    mov ax, si                                ; 89 f0                       ; 0xc2bca
    call 03196h                               ; e8 c7 05                    ; 0xc2bcc
    inc cx                                    ; 41                          ; 0xc2bcf
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2bd0 vgabios.c:1833
    jmp short 02ba5h                          ; eb d0                       ; 0xc2bd3
    mov dx, 003dah                            ; ba da 03                    ; 0xc2bd5 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc2bd8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bd9
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2bdb vgabios.c:1836
    jmp short 02be8h                          ; eb 06                       ; 0xc2be0
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2be2
    jnbe short 02c06h                         ; 77 1e                       ; 0xc2be6
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2be8 vgabios.c:1837
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2beb
    out DX, AL                                ; ee                          ; 0xc2bee
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2bef vgabios.c:1838
    in AL, DX                                 ; ec                          ; 0xc2bf2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bf3
    xor ah, ah                                ; 30 e4                       ; 0xc2bf5
    mov bx, ax                                ; 89 c3                       ; 0xc2bf7
    mov dx, cx                                ; 89 ca                       ; 0xc2bf9
    mov ax, si                                ; 89 f0                       ; 0xc2bfb
    call 03196h                               ; e8 96 05                    ; 0xc2bfd
    inc cx                                    ; 41                          ; 0xc2c00
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2c01 vgabios.c:1839
    jmp short 02be2h                          ; eb dc                       ; 0xc2c04
    mov bx, di                                ; 89 fb                       ; 0xc2c06 vgabios.c:1841
    mov dx, cx                                ; 89 ca                       ; 0xc2c08
    mov ax, si                                ; 89 f0                       ; 0xc2c0a
    call 031b2h                               ; e8 a3 05                    ; 0xc2c0c
    inc cx                                    ; 41                          ; 0xc2c0f vgabios.c:1844
    inc cx                                    ; 41                          ; 0xc2c10
    xor bx, bx                                ; 31 db                       ; 0xc2c11
    mov dx, cx                                ; 89 ca                       ; 0xc2c13
    mov ax, si                                ; 89 f0                       ; 0xc2c15
    call 03196h                               ; e8 7c 05                    ; 0xc2c17
    inc cx                                    ; 41                          ; 0xc2c1a vgabios.c:1845
    xor bx, bx                                ; 31 db                       ; 0xc2c1b
    mov dx, cx                                ; 89 ca                       ; 0xc2c1d
    mov ax, si                                ; 89 f0                       ; 0xc2c1f
    call 03196h                               ; e8 72 05                    ; 0xc2c21
    inc cx                                    ; 41                          ; 0xc2c24 vgabios.c:1846
    xor bx, bx                                ; 31 db                       ; 0xc2c25
    mov dx, cx                                ; 89 ca                       ; 0xc2c27
    mov ax, si                                ; 89 f0                       ; 0xc2c29
    call 03196h                               ; e8 68 05                    ; 0xc2c2b
    inc cx                                    ; 41                          ; 0xc2c2e vgabios.c:1847
    xor bx, bx                                ; 31 db                       ; 0xc2c2f
    mov dx, cx                                ; 89 ca                       ; 0xc2c31
    mov ax, si                                ; 89 f0                       ; 0xc2c33
    call 03196h                               ; e8 5e 05                    ; 0xc2c35
    inc cx                                    ; 41                          ; 0xc2c38
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc2c39 vgabios.c:1849
    jne short 02c42h                          ; 75 03                       ; 0xc2c3d
    jmp near 02dafh                           ; e9 6d 01                    ; 0xc2c3f
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2c42 vgabios.c:1850
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c45
    call 03188h                               ; e8 3d 05                    ; 0xc2c48
    xor ah, ah                                ; 30 e4                       ; 0xc2c4b
    mov bx, ax                                ; 89 c3                       ; 0xc2c4d
    mov dx, cx                                ; 89 ca                       ; 0xc2c4f
    mov ax, si                                ; 89 f0                       ; 0xc2c51
    call 03196h                               ; e8 40 05                    ; 0xc2c53
    inc cx                                    ; 41                          ; 0xc2c56 vgabios.c:1851
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2c57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c5a
    call 031a4h                               ; e8 44 05                    ; 0xc2c5d
    mov bx, ax                                ; 89 c3                       ; 0xc2c60
    mov dx, cx                                ; 89 ca                       ; 0xc2c62
    mov ax, si                                ; 89 f0                       ; 0xc2c64
    call 031b2h                               ; e8 49 05                    ; 0xc2c66
    inc cx                                    ; 41                          ; 0xc2c69 vgabios.c:1852
    inc cx                                    ; 41                          ; 0xc2c6a
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2c6b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c6e
    call 031a4h                               ; e8 30 05                    ; 0xc2c71
    mov bx, ax                                ; 89 c3                       ; 0xc2c74
    mov dx, cx                                ; 89 ca                       ; 0xc2c76
    mov ax, si                                ; 89 f0                       ; 0xc2c78
    call 031b2h                               ; e8 35 05                    ; 0xc2c7a
    inc cx                                    ; 41                          ; 0xc2c7d vgabios.c:1853
    inc cx                                    ; 41                          ; 0xc2c7e
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2c7f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c82
    call 031a4h                               ; e8 1c 05                    ; 0xc2c85
    mov bx, ax                                ; 89 c3                       ; 0xc2c88
    mov dx, cx                                ; 89 ca                       ; 0xc2c8a
    mov ax, si                                ; 89 f0                       ; 0xc2c8c
    call 031b2h                               ; e8 21 05                    ; 0xc2c8e
    inc cx                                    ; 41                          ; 0xc2c91 vgabios.c:1854
    inc cx                                    ; 41                          ; 0xc2c92
    mov dx, 00084h                            ; ba 84 00                    ; 0xc2c93
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c96
    call 03188h                               ; e8 ec 04                    ; 0xc2c99
    xor ah, ah                                ; 30 e4                       ; 0xc2c9c
    mov bx, ax                                ; 89 c3                       ; 0xc2c9e
    mov dx, cx                                ; 89 ca                       ; 0xc2ca0
    mov ax, si                                ; 89 f0                       ; 0xc2ca2
    call 03196h                               ; e8 ef 04                    ; 0xc2ca4
    inc cx                                    ; 41                          ; 0xc2ca7 vgabios.c:1855
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2ca8
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cab
    call 031a4h                               ; e8 f3 04                    ; 0xc2cae
    mov bx, ax                                ; 89 c3                       ; 0xc2cb1
    mov dx, cx                                ; 89 ca                       ; 0xc2cb3
    mov ax, si                                ; 89 f0                       ; 0xc2cb5
    call 031b2h                               ; e8 f8 04                    ; 0xc2cb7
    inc cx                                    ; 41                          ; 0xc2cba vgabios.c:1856
    inc cx                                    ; 41                          ; 0xc2cbb
    mov dx, 00087h                            ; ba 87 00                    ; 0xc2cbc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cbf
    call 03188h                               ; e8 c3 04                    ; 0xc2cc2
    xor ah, ah                                ; 30 e4                       ; 0xc2cc5
    mov bx, ax                                ; 89 c3                       ; 0xc2cc7
    mov dx, cx                                ; 89 ca                       ; 0xc2cc9
    mov ax, si                                ; 89 f0                       ; 0xc2ccb
    call 03196h                               ; e8 c6 04                    ; 0xc2ccd
    inc cx                                    ; 41                          ; 0xc2cd0 vgabios.c:1857
    mov dx, 00088h                            ; ba 88 00                    ; 0xc2cd1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cd4
    call 03188h                               ; e8 ae 04                    ; 0xc2cd7
    mov bl, al                                ; 88 c3                       ; 0xc2cda
    xor bh, bh                                ; 30 ff                       ; 0xc2cdc
    mov dx, cx                                ; 89 ca                       ; 0xc2cde
    mov ax, si                                ; 89 f0                       ; 0xc2ce0
    call 03196h                               ; e8 b1 04                    ; 0xc2ce2
    inc cx                                    ; 41                          ; 0xc2ce5 vgabios.c:1858
    mov dx, 00089h                            ; ba 89 00                    ; 0xc2ce6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ce9
    call 03188h                               ; e8 99 04                    ; 0xc2cec
    xor ah, ah                                ; 30 e4                       ; 0xc2cef
    mov bx, ax                                ; 89 c3                       ; 0xc2cf1
    mov dx, cx                                ; 89 ca                       ; 0xc2cf3
    mov ax, si                                ; 89 f0                       ; 0xc2cf5
    call 03196h                               ; e8 9c 04                    ; 0xc2cf7
    inc cx                                    ; 41                          ; 0xc2cfa vgabios.c:1859
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc2cfb
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cfe
    call 031a4h                               ; e8 a0 04                    ; 0xc2d01
    mov bx, ax                                ; 89 c3                       ; 0xc2d04
    mov dx, cx                                ; 89 ca                       ; 0xc2d06
    mov ax, si                                ; 89 f0                       ; 0xc2d08
    call 031b2h                               ; e8 a5 04                    ; 0xc2d0a
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2d0d vgabios.c:1860
    inc cx                                    ; 41                          ; 0xc2d12
    inc cx                                    ; 41                          ; 0xc2d13
    jmp short 02d1ch                          ; eb 06                       ; 0xc2d14
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2d16
    jnc short 02d3ah                          ; 73 1e                       ; 0xc2d1a
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2d1c vgabios.c:1861
    add dx, dx                                ; 01 d2                       ; 0xc2d1f
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc2d21
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d24
    call 031a4h                               ; e8 7a 04                    ; 0xc2d27
    mov bx, ax                                ; 89 c3                       ; 0xc2d2a
    mov dx, cx                                ; 89 ca                       ; 0xc2d2c
    mov ax, si                                ; 89 f0                       ; 0xc2d2e
    call 031b2h                               ; e8 7f 04                    ; 0xc2d30
    inc cx                                    ; 41                          ; 0xc2d33 vgabios.c:1862
    inc cx                                    ; 41                          ; 0xc2d34
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2d35 vgabios.c:1863
    jmp short 02d16h                          ; eb dc                       ; 0xc2d38
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc2d3a vgabios.c:1864
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d3d
    call 031a4h                               ; e8 61 04                    ; 0xc2d40
    mov bx, ax                                ; 89 c3                       ; 0xc2d43
    mov dx, cx                                ; 89 ca                       ; 0xc2d45
    mov ax, si                                ; 89 f0                       ; 0xc2d47
    call 031b2h                               ; e8 66 04                    ; 0xc2d49
    inc cx                                    ; 41                          ; 0xc2d4c vgabios.c:1865
    inc cx                                    ; 41                          ; 0xc2d4d
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2d4e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d51
    call 03188h                               ; e8 31 04                    ; 0xc2d54
    xor ah, ah                                ; 30 e4                       ; 0xc2d57
    mov bx, ax                                ; 89 c3                       ; 0xc2d59
    mov dx, cx                                ; 89 ca                       ; 0xc2d5b
    mov ax, si                                ; 89 f0                       ; 0xc2d5d
    call 03196h                               ; e8 34 04                    ; 0xc2d5f
    inc cx                                    ; 41                          ; 0xc2d62 vgabios.c:1867
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc2d63
    xor ax, ax                                ; 31 c0                       ; 0xc2d66
    call 031a4h                               ; e8 39 04                    ; 0xc2d68
    mov bx, ax                                ; 89 c3                       ; 0xc2d6b
    mov dx, cx                                ; 89 ca                       ; 0xc2d6d
    mov ax, si                                ; 89 f0                       ; 0xc2d6f
    call 031b2h                               ; e8 3e 04                    ; 0xc2d71
    inc cx                                    ; 41                          ; 0xc2d74 vgabios.c:1868
    inc cx                                    ; 41                          ; 0xc2d75
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc2d76
    xor ax, ax                                ; 31 c0                       ; 0xc2d79
    call 031a4h                               ; e8 26 04                    ; 0xc2d7b
    mov bx, ax                                ; 89 c3                       ; 0xc2d7e
    mov dx, cx                                ; 89 ca                       ; 0xc2d80
    mov ax, si                                ; 89 f0                       ; 0xc2d82
    call 031b2h                               ; e8 2b 04                    ; 0xc2d84
    inc cx                                    ; 41                          ; 0xc2d87 vgabios.c:1869
    inc cx                                    ; 41                          ; 0xc2d88
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc2d89
    xor ax, ax                                ; 31 c0                       ; 0xc2d8c
    call 031a4h                               ; e8 13 04                    ; 0xc2d8e
    mov bx, ax                                ; 89 c3                       ; 0xc2d91
    mov dx, cx                                ; 89 ca                       ; 0xc2d93
    mov ax, si                                ; 89 f0                       ; 0xc2d95
    call 031b2h                               ; e8 18 04                    ; 0xc2d97
    inc cx                                    ; 41                          ; 0xc2d9a vgabios.c:1870
    inc cx                                    ; 41                          ; 0xc2d9b
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc2d9c
    xor ax, ax                                ; 31 c0                       ; 0xc2d9f
    call 031a4h                               ; e8 00 04                    ; 0xc2da1
    mov bx, ax                                ; 89 c3                       ; 0xc2da4
    mov dx, cx                                ; 89 ca                       ; 0xc2da6
    mov ax, si                                ; 89 f0                       ; 0xc2da8
    call 031b2h                               ; e8 05 04                    ; 0xc2daa
    inc cx                                    ; 41                          ; 0xc2dad
    inc cx                                    ; 41                          ; 0xc2dae
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc2daf vgabios.c:1872
    je short 02e20h                           ; 74 6b                       ; 0xc2db3
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc2db5 vgabios.c:1874
    in AL, DX                                 ; ec                          ; 0xc2db8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2db9
    mov bl, al                                ; 88 c3                       ; 0xc2dbb
    xor bh, bh                                ; 30 ff                       ; 0xc2dbd
    mov dx, cx                                ; 89 ca                       ; 0xc2dbf
    mov ax, si                                ; 89 f0                       ; 0xc2dc1
    call 03196h                               ; e8 d0 03                    ; 0xc2dc3
    inc cx                                    ; 41                          ; 0xc2dc6 vgabios.c:1875
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2dc7
    in AL, DX                                 ; ec                          ; 0xc2dca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2dcb
    mov bl, al                                ; 88 c3                       ; 0xc2dcd
    xor bh, bh                                ; 30 ff                       ; 0xc2dcf
    mov dx, cx                                ; 89 ca                       ; 0xc2dd1
    mov ax, si                                ; 89 f0                       ; 0xc2dd3
    call 03196h                               ; e8 be 03                    ; 0xc2dd5
    inc cx                                    ; 41                          ; 0xc2dd8 vgabios.c:1876
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc2dd9
    in AL, DX                                 ; ec                          ; 0xc2ddc
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ddd
    xor ah, ah                                ; 30 e4                       ; 0xc2ddf
    mov bx, ax                                ; 89 c3                       ; 0xc2de1
    mov dx, cx                                ; 89 ca                       ; 0xc2de3
    mov ax, si                                ; 89 f0                       ; 0xc2de5
    call 03196h                               ; e8 ac 03                    ; 0xc2de7
    inc cx                                    ; 41                          ; 0xc2dea vgabios.c:1878
    xor al, al                                ; 30 c0                       ; 0xc2deb
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2ded
    out DX, AL                                ; ee                          ; 0xc2df0
    xor ah, ah                                ; 30 e4                       ; 0xc2df1 vgabios.c:1879
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2df3
    jmp short 02dffh                          ; eb 07                       ; 0xc2df6
    cmp word [bp-00ah], 00300h                ; 81 7e f6 00 03              ; 0xc2df8
    jnc short 02e16h                          ; 73 17                       ; 0xc2dfd
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc2dff vgabios.c:1880
    in AL, DX                                 ; ec                          ; 0xc2e02
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e03
    mov bl, al                                ; 88 c3                       ; 0xc2e05
    xor bh, bh                                ; 30 ff                       ; 0xc2e07
    mov dx, cx                                ; 89 ca                       ; 0xc2e09
    mov ax, si                                ; 89 f0                       ; 0xc2e0b
    call 03196h                               ; e8 86 03                    ; 0xc2e0d
    inc cx                                    ; 41                          ; 0xc2e10
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2e11 vgabios.c:1881
    jmp short 02df8h                          ; eb e2                       ; 0xc2e14
    xor bx, bx                                ; 31 db                       ; 0xc2e16 vgabios.c:1882
    mov dx, cx                                ; 89 ca                       ; 0xc2e18
    mov ax, si                                ; 89 f0                       ; 0xc2e1a
    call 03196h                               ; e8 77 03                    ; 0xc2e1c
    inc cx                                    ; 41                          ; 0xc2e1f
    mov ax, cx                                ; 89 c8                       ; 0xc2e20 vgabios.c:1885
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2e22
    pop di                                    ; 5f                          ; 0xc2e25
    pop si                                    ; 5e                          ; 0xc2e26
    pop cx                                    ; 59                          ; 0xc2e27
    pop bp                                    ; 5d                          ; 0xc2e28
    retn                                      ; c3                          ; 0xc2e29
  ; disGetNextSymbol 0xc2e2a LB 0xec3 -> off=0x0 cb=0000000000000336 uValue=00000000000c2e2a 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc2e2a LB 0x336
    push bp                                   ; 55                          ; 0xc2e2a vgabios.c:1887
    mov bp, sp                                ; 89 e5                       ; 0xc2e2b
    push cx                                   ; 51                          ; 0xc2e2d
    push si                                   ; 56                          ; 0xc2e2e
    push di                                   ; 57                          ; 0xc2e2f
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc2e30
    push ax                                   ; 50                          ; 0xc2e33
    mov si, dx                                ; 89 d6                       ; 0xc2e34
    mov cx, bx                                ; 89 d9                       ; 0xc2e36
    test byte [bp-00eh], 001h                 ; f6 46 f2 01                 ; 0xc2e38 vgabios.c:1891
    je short 02e95h                           ; 74 57                       ; 0xc2e3c
    mov dx, 003dah                            ; ba da 03                    ; 0xc2e3e vgabios.c:1893
    in AL, DX                                 ; ec                          ; 0xc2e41
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e42
    lea dx, [bx+040h]                         ; 8d 57 40                    ; 0xc2e44 vgabios.c:1895
    mov ax, si                                ; 89 f0                       ; 0xc2e47
    call 031a4h                               ; e8 58 03                    ; 0xc2e49
    mov di, ax                                ; 89 c7                       ; 0xc2e4c
    mov word [bp-00ah], strict word 00001h    ; c7 46 f6 01 00              ; 0xc2e4e vgabios.c:1899
    lea cx, [bx+005h]                         ; 8d 4f 05                    ; 0xc2e53 vgabios.c:1897
    jmp short 02e5eh                          ; eb 06                       ; 0xc2e56
    cmp word [bp-00ah], strict byte 00004h    ; 83 7e f6 04                 ; 0xc2e58
    jnbe short 02e76h                         ; 77 18                       ; 0xc2e5c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2e5e vgabios.c:1900
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e61
    out DX, AL                                ; ee                          ; 0xc2e64
    mov dx, cx                                ; 89 ca                       ; 0xc2e65 vgabios.c:1901
    mov ax, si                                ; 89 f0                       ; 0xc2e67
    call 03188h                               ; e8 1c 03                    ; 0xc2e69
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2e6c
    out DX, AL                                ; ee                          ; 0xc2e6f
    inc cx                                    ; 41                          ; 0xc2e70
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2e71 vgabios.c:1902
    jmp short 02e58h                          ; eb e2                       ; 0xc2e74
    xor al, al                                ; 30 c0                       ; 0xc2e76 vgabios.c:1903
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e78
    out DX, AL                                ; ee                          ; 0xc2e7b
    mov dx, cx                                ; 89 ca                       ; 0xc2e7c vgabios.c:1904
    mov ax, si                                ; 89 f0                       ; 0xc2e7e
    call 03188h                               ; e8 05 03                    ; 0xc2e80
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2e83
    out DX, AL                                ; ee                          ; 0xc2e86
    inc cx                                    ; 41                          ; 0xc2e87 vgabios.c:1907
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc2e88
    mov dx, di                                ; 89 fa                       ; 0xc2e8b
    out DX, ax                                ; ef                          ; 0xc2e8d
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2e8e vgabios.c:1909
    jmp short 02e9eh                          ; eb 09                       ; 0xc2e93
    jmp near 02f8bh                           ; e9 f3 00                    ; 0xc2e95
    cmp word [bp-00ah], strict byte 00018h    ; 83 7e f6 18                 ; 0xc2e98
    jnbe short 02ebbh                         ; 77 1d                       ; 0xc2e9c
    cmp word [bp-00ah], strict byte 00011h    ; 83 7e f6 11                 ; 0xc2e9e vgabios.c:1910
    je short 02eb5h                           ; 74 11                       ; 0xc2ea2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2ea4 vgabios.c:1911
    mov dx, di                                ; 89 fa                       ; 0xc2ea7
    out DX, AL                                ; ee                          ; 0xc2ea9
    mov dx, cx                                ; 89 ca                       ; 0xc2eaa vgabios.c:1912
    mov ax, si                                ; 89 f0                       ; 0xc2eac
    call 03188h                               ; e8 d7 02                    ; 0xc2eae
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2eb1
    out DX, AL                                ; ee                          ; 0xc2eb4
    inc cx                                    ; 41                          ; 0xc2eb5 vgabios.c:1914
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2eb6 vgabios.c:1915
    jmp short 02e98h                          ; eb dd                       ; 0xc2eb9
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2ebb vgabios.c:1917
    in AL, DX                                 ; ec                          ; 0xc2ebe
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ebf
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc2ec1
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2ec3
    cmp di, 003d4h                            ; 81 ff d4 03                 ; 0xc2ec6 vgabios.c:1918
    jne short 02ed0h                          ; 75 04                       ; 0xc2eca
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2ecc vgabios.c:1919
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ed0 vgabios.c:1920
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc2ed3
    out DX, AL                                ; ee                          ; 0xc2ed6
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc2ed7 vgabios.c:1923
    mov dx, di                                ; 89 fa                       ; 0xc2ed9
    out DX, AL                                ; ee                          ; 0xc2edb
    mov dx, cx                                ; 89 ca                       ; 0xc2edc vgabios.c:1924
    add dx, strict byte 0fff9h                ; 83 c2 f9                    ; 0xc2ede
    mov ax, si                                ; 89 f0                       ; 0xc2ee1
    call 03188h                               ; e8 a2 02                    ; 0xc2ee3
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2ee6
    out DX, AL                                ; ee                          ; 0xc2ee9
    lea dx, [bx+003h]                         ; 8d 57 03                    ; 0xc2eea vgabios.c:1927
    mov ax, si                                ; 89 f0                       ; 0xc2eed
    call 03188h                               ; e8 96 02                    ; 0xc2eef
    xor ah, ah                                ; 30 e4                       ; 0xc2ef2
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc2ef4
    mov dx, 003dah                            ; ba da 03                    ; 0xc2ef7 vgabios.c:1928
    in AL, DX                                 ; ec                          ; 0xc2efa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2efb
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2efd vgabios.c:1929
    jmp short 02f0ah                          ; eb 06                       ; 0xc2f02
    cmp word [bp-00ah], strict byte 00013h    ; 83 7e f6 13                 ; 0xc2f04
    jnbe short 02f28h                         ; 77 1e                       ; 0xc2f08
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc2f0a vgabios.c:1930
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2f0d
    or ax, word [bp-00ah]                     ; 0b 46 f6                    ; 0xc2f10
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f13
    out DX, AL                                ; ee                          ; 0xc2f16
    mov dx, cx                                ; 89 ca                       ; 0xc2f17 vgabios.c:1931
    mov ax, si                                ; 89 f0                       ; 0xc2f19
    call 03188h                               ; e8 6a 02                    ; 0xc2f1b
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f1e
    out DX, AL                                ; ee                          ; 0xc2f21
    inc cx                                    ; 41                          ; 0xc2f22
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2f23 vgabios.c:1932
    jmp short 02f04h                          ; eb dc                       ; 0xc2f26
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2f28 vgabios.c:1933
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f2b
    out DX, AL                                ; ee                          ; 0xc2f2e
    mov dx, 003dah                            ; ba da 03                    ; 0xc2f2f vgabios.c:1934
    in AL, DX                                 ; ec                          ; 0xc2f32
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f33
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2f35 vgabios.c:1936
    jmp short 02f42h                          ; eb 06                       ; 0xc2f3a
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2f3c
    jnbe short 02f5ah                         ; 77 18                       ; 0xc2f40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2f42 vgabios.c:1937
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2f45
    out DX, AL                                ; ee                          ; 0xc2f48
    mov dx, cx                                ; 89 ca                       ; 0xc2f49 vgabios.c:1938
    mov ax, si                                ; 89 f0                       ; 0xc2f4b
    call 03188h                               ; e8 38 02                    ; 0xc2f4d
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2f50
    out DX, AL                                ; ee                          ; 0xc2f53
    inc cx                                    ; 41                          ; 0xc2f54
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2f55 vgabios.c:1939
    jmp short 02f3ch                          ; eb e2                       ; 0xc2f58
    add cx, strict byte 00006h                ; 83 c1 06                    ; 0xc2f5a vgabios.c:1940
    mov dx, bx                                ; 89 da                       ; 0xc2f5d vgabios.c:1941
    mov ax, si                                ; 89 f0                       ; 0xc2f5f
    call 03188h                               ; e8 24 02                    ; 0xc2f61
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2f64
    out DX, AL                                ; ee                          ; 0xc2f67
    inc bx                                    ; 43                          ; 0xc2f68
    mov dx, bx                                ; 89 da                       ; 0xc2f69 vgabios.c:1944
    mov ax, si                                ; 89 f0                       ; 0xc2f6b
    call 03188h                               ; e8 18 02                    ; 0xc2f6d
    mov dx, di                                ; 89 fa                       ; 0xc2f70
    out DX, AL                                ; ee                          ; 0xc2f72
    inc bx                                    ; 43                          ; 0xc2f73
    mov dx, bx                                ; 89 da                       ; 0xc2f74 vgabios.c:1945
    mov ax, si                                ; 89 f0                       ; 0xc2f76
    call 03188h                               ; e8 0d 02                    ; 0xc2f78
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2f7b
    out DX, AL                                ; ee                          ; 0xc2f7e
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc2f7f
    mov ax, si                                ; 89 f0                       ; 0xc2f82
    call 03188h                               ; e8 01 02                    ; 0xc2f84
    lea dx, [di+006h]                         ; 8d 55 06                    ; 0xc2f87
    out DX, AL                                ; ee                          ; 0xc2f8a
    test byte [bp-00eh], 002h                 ; f6 46 f2 02                 ; 0xc2f8b vgabios.c:1949
    jne short 02f94h                          ; 75 03                       ; 0xc2f8f
    jmp near 03109h                           ; e9 75 01                    ; 0xc2f91
    mov dx, cx                                ; 89 ca                       ; 0xc2f94 vgabios.c:1950
    mov ax, si                                ; 89 f0                       ; 0xc2f96
    call 03188h                               ; e8 ed 01                    ; 0xc2f98
    xor ah, ah                                ; 30 e4                       ; 0xc2f9b
    mov bx, ax                                ; 89 c3                       ; 0xc2f9d
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2f9f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fa2
    call 03196h                               ; e8 ee 01                    ; 0xc2fa5
    inc cx                                    ; 41                          ; 0xc2fa8
    mov dx, cx                                ; 89 ca                       ; 0xc2fa9 vgabios.c:1951
    mov ax, si                                ; 89 f0                       ; 0xc2fab
    call 031a4h                               ; e8 f4 01                    ; 0xc2fad
    mov bx, ax                                ; 89 c3                       ; 0xc2fb0
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2fb2
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fb5
    call 031b2h                               ; e8 f7 01                    ; 0xc2fb8
    inc cx                                    ; 41                          ; 0xc2fbb
    inc cx                                    ; 41                          ; 0xc2fbc
    mov dx, cx                                ; 89 ca                       ; 0xc2fbd vgabios.c:1952
    mov ax, si                                ; 89 f0                       ; 0xc2fbf
    call 031a4h                               ; e8 e0 01                    ; 0xc2fc1
    mov bx, ax                                ; 89 c3                       ; 0xc2fc4
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2fc6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fc9
    call 031b2h                               ; e8 e3 01                    ; 0xc2fcc
    inc cx                                    ; 41                          ; 0xc2fcf
    inc cx                                    ; 41                          ; 0xc2fd0
    mov dx, cx                                ; 89 ca                       ; 0xc2fd1 vgabios.c:1953
    mov ax, si                                ; 89 f0                       ; 0xc2fd3
    call 031a4h                               ; e8 cc 01                    ; 0xc2fd5
    mov bx, ax                                ; 89 c3                       ; 0xc2fd8
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2fda
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fdd
    call 031b2h                               ; e8 cf 01                    ; 0xc2fe0
    inc cx                                    ; 41                          ; 0xc2fe3
    inc cx                                    ; 41                          ; 0xc2fe4
    mov dx, cx                                ; 89 ca                       ; 0xc2fe5 vgabios.c:1954
    mov ax, si                                ; 89 f0                       ; 0xc2fe7
    call 03188h                               ; e8 9c 01                    ; 0xc2fe9
    xor ah, ah                                ; 30 e4                       ; 0xc2fec
    mov bx, ax                                ; 89 c3                       ; 0xc2fee
    mov dx, 00084h                            ; ba 84 00                    ; 0xc2ff0
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ff3
    call 03196h                               ; e8 9d 01                    ; 0xc2ff6
    inc cx                                    ; 41                          ; 0xc2ff9
    mov dx, cx                                ; 89 ca                       ; 0xc2ffa vgabios.c:1955
    mov ax, si                                ; 89 f0                       ; 0xc2ffc
    call 031a4h                               ; e8 a3 01                    ; 0xc2ffe
    mov bx, ax                                ; 89 c3                       ; 0xc3001
    mov dx, 00085h                            ; ba 85 00                    ; 0xc3003
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3006
    call 031b2h                               ; e8 a6 01                    ; 0xc3009
    inc cx                                    ; 41                          ; 0xc300c
    inc cx                                    ; 41                          ; 0xc300d
    mov dx, cx                                ; 89 ca                       ; 0xc300e vgabios.c:1956
    mov ax, si                                ; 89 f0                       ; 0xc3010
    call 03188h                               ; e8 73 01                    ; 0xc3012
    mov dl, al                                ; 88 c2                       ; 0xc3015
    xor dh, dh                                ; 30 f6                       ; 0xc3017
    mov bx, dx                                ; 89 d3                       ; 0xc3019
    mov dx, 00087h                            ; ba 87 00                    ; 0xc301b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc301e
    call 03196h                               ; e8 72 01                    ; 0xc3021
    inc cx                                    ; 41                          ; 0xc3024
    mov dx, cx                                ; 89 ca                       ; 0xc3025 vgabios.c:1957
    mov ax, si                                ; 89 f0                       ; 0xc3027
    call 03188h                               ; e8 5c 01                    ; 0xc3029
    mov dl, al                                ; 88 c2                       ; 0xc302c
    xor dh, dh                                ; 30 f6                       ; 0xc302e
    mov bx, dx                                ; 89 d3                       ; 0xc3030
    mov dx, 00088h                            ; ba 88 00                    ; 0xc3032
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3035
    call 03196h                               ; e8 5b 01                    ; 0xc3038
    inc cx                                    ; 41                          ; 0xc303b
    mov dx, cx                                ; 89 ca                       ; 0xc303c vgabios.c:1958
    mov ax, si                                ; 89 f0                       ; 0xc303e
    call 03188h                               ; e8 45 01                    ; 0xc3040
    mov dl, al                                ; 88 c2                       ; 0xc3043
    xor dh, dh                                ; 30 f6                       ; 0xc3045
    mov bx, dx                                ; 89 d3                       ; 0xc3047
    mov dx, 00089h                            ; ba 89 00                    ; 0xc3049
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc304c
    call 03196h                               ; e8 44 01                    ; 0xc304f
    inc cx                                    ; 41                          ; 0xc3052
    mov dx, cx                                ; 89 ca                       ; 0xc3053 vgabios.c:1959
    mov ax, si                                ; 89 f0                       ; 0xc3055
    call 031a4h                               ; e8 4a 01                    ; 0xc3057
    mov bx, ax                                ; 89 c3                       ; 0xc305a
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc305c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc305f
    call 031b2h                               ; e8 4d 01                    ; 0xc3062
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc3065 vgabios.c:1960
    inc cx                                    ; 41                          ; 0xc306a
    inc cx                                    ; 41                          ; 0xc306b
    jmp short 03074h                          ; eb 06                       ; 0xc306c
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc306e
    jnc short 03092h                          ; 73 1e                       ; 0xc3072
    mov dx, cx                                ; 89 ca                       ; 0xc3074 vgabios.c:1961
    mov ax, si                                ; 89 f0                       ; 0xc3076
    call 031a4h                               ; e8 29 01                    ; 0xc3078
    mov bx, ax                                ; 89 c3                       ; 0xc307b
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc307d
    add dx, dx                                ; 01 d2                       ; 0xc3080
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc3082
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3085
    call 031b2h                               ; e8 27 01                    ; 0xc3088
    inc cx                                    ; 41                          ; 0xc308b vgabios.c:1962
    inc cx                                    ; 41                          ; 0xc308c
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc308d vgabios.c:1963
    jmp short 0306eh                          ; eb dc                       ; 0xc3090
    mov dx, cx                                ; 89 ca                       ; 0xc3092 vgabios.c:1964
    mov ax, si                                ; 89 f0                       ; 0xc3094
    call 031a4h                               ; e8 0b 01                    ; 0xc3096
    mov bx, ax                                ; 89 c3                       ; 0xc3099
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc309b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc309e
    call 031b2h                               ; e8 0e 01                    ; 0xc30a1
    inc cx                                    ; 41                          ; 0xc30a4
    inc cx                                    ; 41                          ; 0xc30a5
    mov dx, cx                                ; 89 ca                       ; 0xc30a6 vgabios.c:1965
    mov ax, si                                ; 89 f0                       ; 0xc30a8
    call 03188h                               ; e8 db 00                    ; 0xc30aa
    mov dl, al                                ; 88 c2                       ; 0xc30ad
    xor dh, dh                                ; 30 f6                       ; 0xc30af
    mov bx, dx                                ; 89 d3                       ; 0xc30b1
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc30b3
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30b6
    call 03196h                               ; e8 da 00                    ; 0xc30b9
    inc cx                                    ; 41                          ; 0xc30bc
    mov dx, cx                                ; 89 ca                       ; 0xc30bd vgabios.c:1967
    mov ax, si                                ; 89 f0                       ; 0xc30bf
    call 031a4h                               ; e8 e0 00                    ; 0xc30c1
    mov bx, ax                                ; 89 c3                       ; 0xc30c4
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc30c6
    xor ax, ax                                ; 31 c0                       ; 0xc30c9
    call 031b2h                               ; e8 e4 00                    ; 0xc30cb
    inc cx                                    ; 41                          ; 0xc30ce
    inc cx                                    ; 41                          ; 0xc30cf
    mov dx, cx                                ; 89 ca                       ; 0xc30d0 vgabios.c:1968
    mov ax, si                                ; 89 f0                       ; 0xc30d2
    call 031a4h                               ; e8 cd 00                    ; 0xc30d4
    mov bx, ax                                ; 89 c3                       ; 0xc30d7
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc30d9
    xor ax, ax                                ; 31 c0                       ; 0xc30dc
    call 031b2h                               ; e8 d1 00                    ; 0xc30de
    inc cx                                    ; 41                          ; 0xc30e1
    inc cx                                    ; 41                          ; 0xc30e2
    mov dx, cx                                ; 89 ca                       ; 0xc30e3 vgabios.c:1969
    mov ax, si                                ; 89 f0                       ; 0xc30e5
    call 031a4h                               ; e8 ba 00                    ; 0xc30e7
    mov bx, ax                                ; 89 c3                       ; 0xc30ea
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc30ec
    xor ax, ax                                ; 31 c0                       ; 0xc30ef
    call 031b2h                               ; e8 be 00                    ; 0xc30f1
    inc cx                                    ; 41                          ; 0xc30f4
    inc cx                                    ; 41                          ; 0xc30f5
    mov dx, cx                                ; 89 ca                       ; 0xc30f6 vgabios.c:1970
    mov ax, si                                ; 89 f0                       ; 0xc30f8
    call 031a4h                               ; e8 a7 00                    ; 0xc30fa
    mov bx, ax                                ; 89 c3                       ; 0xc30fd
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc30ff
    xor ax, ax                                ; 31 c0                       ; 0xc3102
    call 031b2h                               ; e8 ab 00                    ; 0xc3104
    inc cx                                    ; 41                          ; 0xc3107
    inc cx                                    ; 41                          ; 0xc3108
    test byte [bp-00eh], 004h                 ; f6 46 f2 04                 ; 0xc3109 vgabios.c:1972
    je short 03156h                           ; 74 47                       ; 0xc310d
    inc cx                                    ; 41                          ; 0xc310f vgabios.c:1973
    mov dx, cx                                ; 89 ca                       ; 0xc3110 vgabios.c:1974
    mov ax, si                                ; 89 f0                       ; 0xc3112
    call 03188h                               ; e8 71 00                    ; 0xc3114
    xor ah, ah                                ; 30 e4                       ; 0xc3117
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3119
    inc cx                                    ; 41                          ; 0xc311c
    mov dx, cx                                ; 89 ca                       ; 0xc311d vgabios.c:1975
    mov ax, si                                ; 89 f0                       ; 0xc311f
    call 03188h                               ; e8 64 00                    ; 0xc3121
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3124
    out DX, AL                                ; ee                          ; 0xc3127
    inc cx                                    ; 41                          ; 0xc3128 vgabios.c:1977
    xor al, al                                ; 30 c0                       ; 0xc3129
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc312b
    out DX, AL                                ; ee                          ; 0xc312e
    xor ah, ah                                ; 30 e4                       ; 0xc312f vgabios.c:1978
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3131
    jmp short 0313dh                          ; eb 07                       ; 0xc3134
    cmp word [bp-00ah], 00300h                ; 81 7e f6 00 03              ; 0xc3136
    jnc short 0314eh                          ; 73 11                       ; 0xc313b
    mov dx, cx                                ; 89 ca                       ; 0xc313d vgabios.c:1979
    mov ax, si                                ; 89 f0                       ; 0xc313f
    call 03188h                               ; e8 44 00                    ; 0xc3141
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3144
    out DX, AL                                ; ee                          ; 0xc3147
    inc cx                                    ; 41                          ; 0xc3148
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc3149 vgabios.c:1980
    jmp short 03136h                          ; eb e8                       ; 0xc314c
    inc cx                                    ; 41                          ; 0xc314e vgabios.c:1981
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc314f
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3152
    out DX, AL                                ; ee                          ; 0xc3155
    mov ax, cx                                ; 89 c8                       ; 0xc3156 vgabios.c:1985
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3158
    pop di                                    ; 5f                          ; 0xc315b
    pop si                                    ; 5e                          ; 0xc315c
    pop cx                                    ; 59                          ; 0xc315d
    pop bp                                    ; 5d                          ; 0xc315e
    retn                                      ; c3                          ; 0xc315f
  ; disGetNextSymbol 0xc3160 LB 0xb8d -> off=0x0 cb=0000000000000028 uValue=00000000000c3160 'find_vga_entry'
find_vga_entry:                              ; 0xc3160 LB 0x28
    push bx                                   ; 53                          ; 0xc3160 vgabios.c:1994
    push dx                                   ; 52                          ; 0xc3161
    push bp                                   ; 55                          ; 0xc3162
    mov bp, sp                                ; 89 e5                       ; 0xc3163
    mov dl, al                                ; 88 c2                       ; 0xc3165
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc3167 vgabios.c:1996
    xor al, al                                ; 30 c0                       ; 0xc3169 vgabios.c:1997
    jmp short 03173h                          ; eb 06                       ; 0xc316b
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc316d vgabios.c:1998
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc316f
    jnbe short 03182h                         ; 77 0f                       ; 0xc3171
    mov bl, al                                ; 88 c3                       ; 0xc3173
    xor bh, bh                                ; 30 ff                       ; 0xc3175
    sal bx, 003h                              ; c1 e3 03                    ; 0xc3177
    cmp dl, byte [bx+04634h]                  ; 3a 97 34 46                 ; 0xc317a
    jne short 0316dh                          ; 75 ed                       ; 0xc317e
    mov ah, al                                ; 88 c4                       ; 0xc3180
    mov al, ah                                ; 88 e0                       ; 0xc3182 vgabios.c:2003
    pop bp                                    ; 5d                          ; 0xc3184
    pop dx                                    ; 5a                          ; 0xc3185
    pop bx                                    ; 5b                          ; 0xc3186
    retn                                      ; c3                          ; 0xc3187
  ; disGetNextSymbol 0xc3188 LB 0xb65 -> off=0x0 cb=000000000000000e uValue=00000000000c3188 'read_byte'
read_byte:                                   ; 0xc3188 LB 0xe
    push bx                                   ; 53                          ; 0xc3188 vgabios.c:2011
    push bp                                   ; 55                          ; 0xc3189
    mov bp, sp                                ; 89 e5                       ; 0xc318a
    mov bx, dx                                ; 89 d3                       ; 0xc318c
    mov es, ax                                ; 8e c0                       ; 0xc318e vgabios.c:2013
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3190
    pop bp                                    ; 5d                          ; 0xc3193 vgabios.c:2014
    pop bx                                    ; 5b                          ; 0xc3194
    retn                                      ; c3                          ; 0xc3195
  ; disGetNextSymbol 0xc3196 LB 0xb57 -> off=0x0 cb=000000000000000e uValue=00000000000c3196 'write_byte'
write_byte:                                  ; 0xc3196 LB 0xe
    push si                                   ; 56                          ; 0xc3196 vgabios.c:2016
    push bp                                   ; 55                          ; 0xc3197
    mov bp, sp                                ; 89 e5                       ; 0xc3198
    mov si, dx                                ; 89 d6                       ; 0xc319a
    mov es, ax                                ; 8e c0                       ; 0xc319c vgabios.c:2018
    mov byte [es:si], bl                      ; 26 88 1c                    ; 0xc319e
    pop bp                                    ; 5d                          ; 0xc31a1 vgabios.c:2019
    pop si                                    ; 5e                          ; 0xc31a2
    retn                                      ; c3                          ; 0xc31a3
  ; disGetNextSymbol 0xc31a4 LB 0xb49 -> off=0x0 cb=000000000000000e uValue=00000000000c31a4 'read_word'
read_word:                                   ; 0xc31a4 LB 0xe
    push bx                                   ; 53                          ; 0xc31a4 vgabios.c:2021
    push bp                                   ; 55                          ; 0xc31a5
    mov bp, sp                                ; 89 e5                       ; 0xc31a6
    mov bx, dx                                ; 89 d3                       ; 0xc31a8
    mov es, ax                                ; 8e c0                       ; 0xc31aa vgabios.c:2023
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc31ac
    pop bp                                    ; 5d                          ; 0xc31af vgabios.c:2024
    pop bx                                    ; 5b                          ; 0xc31b0
    retn                                      ; c3                          ; 0xc31b1
  ; disGetNextSymbol 0xc31b2 LB 0xb3b -> off=0x0 cb=000000000000000e uValue=00000000000c31b2 'write_word'
write_word:                                  ; 0xc31b2 LB 0xe
    push si                                   ; 56                          ; 0xc31b2 vgabios.c:2026
    push bp                                   ; 55                          ; 0xc31b3
    mov bp, sp                                ; 89 e5                       ; 0xc31b4
    mov si, dx                                ; 89 d6                       ; 0xc31b6
    mov es, ax                                ; 8e c0                       ; 0xc31b8 vgabios.c:2028
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc31ba
    pop bp                                    ; 5d                          ; 0xc31bd vgabios.c:2029
    pop si                                    ; 5e                          ; 0xc31be
    retn                                      ; c3                          ; 0xc31bf
  ; disGetNextSymbol 0xc31c0 LB 0xb2d -> off=0x0 cb=0000000000000012 uValue=00000000000c31c0 'read_dword'
read_dword:                                  ; 0xc31c0 LB 0x12
    push bx                                   ; 53                          ; 0xc31c0 vgabios.c:2031
    push bp                                   ; 55                          ; 0xc31c1
    mov bp, sp                                ; 89 e5                       ; 0xc31c2
    mov bx, dx                                ; 89 d3                       ; 0xc31c4
    mov es, ax                                ; 8e c0                       ; 0xc31c6 vgabios.c:2033
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc31c8
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc31cb
    pop bp                                    ; 5d                          ; 0xc31cf vgabios.c:2034
    pop bx                                    ; 5b                          ; 0xc31d0
    retn                                      ; c3                          ; 0xc31d1
  ; disGetNextSymbol 0xc31d2 LB 0xb1b -> off=0x0 cb=0000000000000012 uValue=00000000000c31d2 'write_dword'
write_dword:                                 ; 0xc31d2 LB 0x12
    push si                                   ; 56                          ; 0xc31d2 vgabios.c:2036
    push bp                                   ; 55                          ; 0xc31d3
    mov bp, sp                                ; 89 e5                       ; 0xc31d4
    mov si, dx                                ; 89 d6                       ; 0xc31d6
    mov es, ax                                ; 8e c0                       ; 0xc31d8 vgabios.c:2038
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc31da
    mov word [es:si+002h], cx                 ; 26 89 4c 02                 ; 0xc31dd
    pop bp                                    ; 5d                          ; 0xc31e1 vgabios.c:2039
    pop si                                    ; 5e                          ; 0xc31e2
    retn                                      ; c3                          ; 0xc31e3
  ; disGetNextSymbol 0xc31e4 LB 0xb09 -> off=0x84 cb=0000000000000390 uValue=00000000000c3268 'int10_func'
    db  04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h, 005h
    db  004h, 003h, 002h, 001h, 000h, 0f1h, 035h, 099h, 032h, 0d6h, 032h, 0e9h, 032h, 0f9h, 032h, 00ch
    db  033h, 01ch, 033h, 023h, 033h, 05bh, 033h, 05fh, 033h, 070h, 033h, 08bh, 033h, 0a6h, 033h, 0beh
    db  033h, 0dbh, 033h, 0efh, 033h, 0fbh, 033h, 0b2h, 034h, 0e7h, 034h, 018h, 035h, 02dh, 035h, 06ah
    db  035h, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h, 001h, 000h, 0f1h
    db  035h, 018h, 034h, 039h, 034h, 048h, 034h, 057h, 034h, 018h, 034h, 039h, 034h, 048h, 034h, 057h
    db  034h, 066h, 034h, 072h, 034h, 08bh, 034h, 090h, 034h, 095h, 034h, 09ah, 034h, 00ah, 009h, 006h
    db  004h, 002h, 001h, 000h, 0e5h, 035h, 090h, 035h, 09dh, 035h, 0adh, 035h, 0bdh, 035h, 0d2h, 035h
    db  0e5h, 035h, 0e5h, 035h
int10_func:                                  ; 0xc3268 LB 0x390
    push bp                                   ; 55                          ; 0xc3268 vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc3269
    push si                                   ; 56                          ; 0xc326b
    push di                                   ; 57                          ; 0xc326c
    push ax                                   ; 50                          ; 0xc326d
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc326e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3271 vgabios.c:2120
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3274
    cmp ax, strict word 0004fh                ; 3d 4f 00                    ; 0xc3277
    jnbe short 032e6h                         ; 77 6a                       ; 0xc327a
    push CS                                   ; 0e                          ; 0xc327c
    pop ES                                    ; 07                          ; 0xc327d
    mov cx, strict word 00016h                ; b9 16 00                    ; 0xc327e
    mov di, 031e4h                            ; bf e4 31                    ; 0xc3281
    repne scasb                               ; f2 ae                       ; 0xc3284
    sal cx, 1                                 ; d1 e1                       ; 0xc3286
    mov di, cx                                ; 89 cf                       ; 0xc3288
    mov bx, word [cs:di+031f9h]               ; 2e 8b 9d f9 31              ; 0xc328a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc328f
    xor ah, ah                                ; 30 e4                       ; 0xc3292
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc3294
    jmp bx                                    ; ff e3                       ; 0xc3297
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3299 vgabios.c:2123
    xor ah, ah                                ; 30 e4                       ; 0xc329c
    call 0101ch                               ; e8 7b dd                    ; 0xc329e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32a1 vgabios.c:2124
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc32a4
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc32a7
    je short 032c1h                           ; 74 15                       ; 0xc32aa
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc32ac
    je short 032b8h                           ; 74 07                       ; 0xc32af
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc32b1
    jbe short 032c1h                          ; 76 0b                       ; 0xc32b4
    jmp short 032cah                          ; eb 12                       ; 0xc32b6
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32b8 vgabios.c:2126
    xor al, al                                ; 30 c0                       ; 0xc32bb
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc32bd
    jmp short 032d1h                          ; eb 10                       ; 0xc32bf vgabios.c:2127
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32c1 vgabios.c:2135
    xor al, al                                ; 30 c0                       ; 0xc32c4
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc32c6
    jmp short 032d1h                          ; eb 07                       ; 0xc32c8
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32ca vgabios.c:2138
    xor al, al                                ; 30 c0                       ; 0xc32cd
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc32cf
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc32d1
    jmp short 032e6h                          ; eb 10                       ; 0xc32d4 vgabios.c:2140
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc32d6 vgabios.c:2142
    mov dx, ax                                ; 89 c2                       ; 0xc32d9
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc32db
    shr ax, 008h                              ; c1 e8 08                    ; 0xc32de
    xor ah, ah                                ; 30 e4                       ; 0xc32e1
    call 00dcbh                               ; e8 e5 da                    ; 0xc32e3
    jmp near 035f1h                           ; e9 08 03                    ; 0xc32e6 vgabios.c:2143
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc32e9 vgabios.c:2145
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc32ec
    shr ax, 008h                              ; c1 e8 08                    ; 0xc32ef
    xor ah, ah                                ; 30 e4                       ; 0xc32f2
    call 00e79h                               ; e8 82 db                    ; 0xc32f4
    jmp short 032e6h                          ; eb ed                       ; 0xc32f7 vgabios.c:2146
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc32f9 vgabios.c:2148
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc32fc
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc32ff
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3302
    xor ah, ah                                ; 30 e4                       ; 0xc3305
    call 00a8bh                               ; e8 81 d7                    ; 0xc3307
    jmp short 032e6h                          ; eb da                       ; 0xc330a vgabios.c:2149
    xor al, al                                ; 30 c0                       ; 0xc330c vgabios.c:2155
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc330e
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3311 vgabios.c:2156
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3314 vgabios.c:2157
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3317 vgabios.c:2158
    jmp short 032e6h                          ; eb ca                       ; 0xc331a vgabios.c:2159
    mov al, dl                                ; 88 d0                       ; 0xc331c vgabios.c:2161
    call 00f2eh                               ; e8 0d dc                    ; 0xc331e
    jmp short 032e6h                          ; eb c3                       ; 0xc3321 vgabios.c:2162
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3323 vgabios.c:2164
    push ax                                   ; 50                          ; 0xc3326
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3327
    push ax                                   ; 50                          ; 0xc332a
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc332b
    xor ah, ah                                ; 30 e4                       ; 0xc332e
    push ax                                   ; 50                          ; 0xc3330
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3331
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3334
    xor ah, ah                                ; 30 e4                       ; 0xc3337
    push ax                                   ; 50                          ; 0xc3339
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc333a
    mov cx, ax                                ; 89 c1                       ; 0xc333d
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc333f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3342
    xor ah, ah                                ; 30 e4                       ; 0xc3345
    mov bx, ax                                ; 89 c3                       ; 0xc3347
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3349
    shr ax, 008h                              ; c1 e8 08                    ; 0xc334c
    xor ah, ah                                ; 30 e4                       ; 0xc334f
    mov dx, ax                                ; 89 c2                       ; 0xc3351
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3353
    call 0166ah                               ; e8 11 e3                    ; 0xc3356
    jmp short 032e6h                          ; eb 8b                       ; 0xc3359 vgabios.c:2165
    xor al, al                                ; 30 c0                       ; 0xc335b vgabios.c:2167
    jmp short 03326h                          ; eb c7                       ; 0xc335d
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc335f vgabios.c:2170
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3362
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3365
    xor ah, ah                                ; 30 e4                       ; 0xc3368
    call 00ad1h                               ; e8 64 d7                    ; 0xc336a
    jmp near 035f1h                           ; e9 81 02                    ; 0xc336d vgabios.c:2171
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3370 vgabios.c:2173
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3373
    mov bx, ax                                ; 89 c3                       ; 0xc3376
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3378
    shr ax, 008h                              ; c1 e8 08                    ; 0xc337b
    xor ah, ah                                ; 30 e4                       ; 0xc337e
    mov dx, ax                                ; 89 c2                       ; 0xc3380
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3382
    call 01ea1h                               ; e8 19 eb                    ; 0xc3385
    jmp near 035f1h                           ; e9 66 02                    ; 0xc3388 vgabios.c:2174
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc338b vgabios.c:2176
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc338e
    mov bx, ax                                ; 89 c3                       ; 0xc3391
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3393
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3396
    xor ah, ah                                ; 30 e4                       ; 0xc3399
    mov dx, ax                                ; 89 c2                       ; 0xc339b
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc339d
    call 0202eh                               ; e8 8b ec                    ; 0xc33a0
    jmp near 035f1h                           ; e9 4b 02                    ; 0xc33a3 vgabios.c:2177
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc33a6 vgabios.c:2179
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc33a9
    mov al, dl                                ; 88 d0                       ; 0xc33ac
    mov dx, ax                                ; 89 c2                       ; 0xc33ae
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc33b0
    shr ax, 008h                              ; c1 e8 08                    ; 0xc33b3
    xor ah, ah                                ; 30 e4                       ; 0xc33b6
    call 021c4h                               ; e8 09 ee                    ; 0xc33b8
    jmp near 035f1h                           ; e9 33 02                    ; 0xc33bb vgabios.c:2180
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc33be vgabios.c:2182
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc33c1
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33c4
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc33c7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc33ca
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc33cd
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc33d0
    xor ah, ah                                ; 30 e4                       ; 0xc33d3
    call 00bfch                               ; e8 24 d8                    ; 0xc33d5
    jmp near 035f1h                           ; e9 16 02                    ; 0xc33d8 vgabios.c:2183
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc33db vgabios.c:2191
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc33de
    mov bx, ax                                ; 89 c3                       ; 0xc33e1
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc33e3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc33e6
    call 0233fh                               ; e8 53 ef                    ; 0xc33e9
    jmp near 035f1h                           ; e9 02 02                    ; 0xc33ec vgabios.c:2192
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33ef vgabios.c:2195
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc33f2
    call 00d3eh                               ; e8 46 d9                    ; 0xc33f5
    jmp near 035f1h                           ; e9 f6 01                    ; 0xc33f8 vgabios.c:2196
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc33fb vgabios.c:2198
    jnbe short 0346fh                         ; 77 6f                       ; 0xc33fe
    push CS                                   ; 0e                          ; 0xc3400
    pop ES                                    ; 07                          ; 0xc3401
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc3402
    mov di, 03225h                            ; bf 25 32                    ; 0xc3405
    repne scasb                               ; f2 ae                       ; 0xc3408
    sal cx, 1                                 ; d1 e1                       ; 0xc340a
    mov di, cx                                ; 89 cf                       ; 0xc340c
    mov dx, word [cs:di+03233h]               ; 2e 8b 95 33 32              ; 0xc340e
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3413
    jmp dx                                    ; ff e2                       ; 0xc3416
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3418 vgabios.c:2202
    shr ax, 008h                              ; c1 e8 08                    ; 0xc341b
    xor ah, ah                                ; 30 e4                       ; 0xc341e
    push ax                                   ; 50                          ; 0xc3420
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3421
    push ax                                   ; 50                          ; 0xc3424
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3425
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3428
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc342b
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc342e
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3431
    call 026d8h                               ; e8 a1 f2                    ; 0xc3434
    jmp short 0346fh                          ; eb 36                       ; 0xc3437 vgabios.c:2203
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3439 vgabios.c:2206
    xor ah, ah                                ; 30 e4                       ; 0xc343c
    mov dx, ax                                ; 89 c2                       ; 0xc343e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3440
    call 02758h                               ; e8 12 f3                    ; 0xc3443
    jmp short 0346fh                          ; eb 27                       ; 0xc3446 vgabios.c:2207
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3448 vgabios.c:2210
    xor ah, ah                                ; 30 e4                       ; 0xc344b
    mov dx, ax                                ; 89 c2                       ; 0xc344d
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc344f
    call 027c6h                               ; e8 71 f3                    ; 0xc3452
    jmp short 0346fh                          ; eb 18                       ; 0xc3455 vgabios.c:2211
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3457 vgabios.c:2214
    xor ah, ah                                ; 30 e4                       ; 0xc345a
    mov dx, ax                                ; 89 c2                       ; 0xc345c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc345e
    call 02836h                               ; e8 d2 f3                    ; 0xc3461
    jmp short 0346fh                          ; eb 09                       ; 0xc3464 vgabios.c:2215
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3466 vgabios.c:2217
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3469
    call 028a6h                               ; e8 37 f4                    ; 0xc346c
    jmp near 035f1h                           ; e9 7f 01                    ; 0xc346f vgabios.c:2218
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3472 vgabios.c:2220
    push ax                                   ; 50                          ; 0xc3475
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3476
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3479
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc347c
    mov si, word [bp+016h]                    ; 8b 76 16                    ; 0xc347f
    mov cx, ax                                ; 89 c1                       ; 0xc3482
    mov ax, si                                ; 89 f0                       ; 0xc3484
    call 028abh                               ; e8 22 f4                    ; 0xc3486
    jmp short 0346fh                          ; eb e4                       ; 0xc3489 vgabios.c:2221
    call 028b2h                               ; e8 24 f4                    ; 0xc348b vgabios.c:2223
    jmp short 0346fh                          ; eb df                       ; 0xc348e vgabios.c:2224
    call 028b7h                               ; e8 24 f4                    ; 0xc3490 vgabios.c:2226
    jmp short 0346fh                          ; eb da                       ; 0xc3493 vgabios.c:2227
    call 028bch                               ; e8 24 f4                    ; 0xc3495 vgabios.c:2229
    jmp short 0346fh                          ; eb d5                       ; 0xc3498 vgabios.c:2230
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc349a vgabios.c:2232
    push ax                                   ; 50                          ; 0xc349d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc349e
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34a1
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc34a4
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc34a7
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc34aa
    call 00b81h                               ; e8 d1 d6                    ; 0xc34ad
    jmp short 0346fh                          ; eb bd                       ; 0xc34b0 vgabios.c:2240
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34b2 vgabios.c:2242
    xor ah, ah                                ; 30 e4                       ; 0xc34b5
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc34b7
    je short 034e0h                           ; 74 24                       ; 0xc34ba
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc34bc
    je short 034cbh                           ; 74 0a                       ; 0xc34bf
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc34c1
    jne short 03515h                          ; 75 4f                       ; 0xc34c4
    call 028c1h                               ; e8 f8 f3                    ; 0xc34c6 vgabios.c:2245
    jmp short 03515h                          ; eb 4a                       ; 0xc34c9 vgabios.c:2246
    mov al, dl                                ; 88 d0                       ; 0xc34cb vgabios.c:2248
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc34cd
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc34d0
    call 028c6h                               ; e8 f0 f3                    ; 0xc34d3
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34d6 vgabios.c:2249
    xor al, al                                ; 30 c0                       ; 0xc34d9
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc34db
    jmp near 032d1h                           ; e9 f1 fd                    ; 0xc34dd
    mov al, dl                                ; 88 d0                       ; 0xc34e0 vgabios.c:2252
    call 028cbh                               ; e8 e6 f3                    ; 0xc34e2
    jmp short 034d6h                          ; eb ef                       ; 0xc34e5
    push word [bp+008h]                       ; ff 76 08                    ; 0xc34e7 vgabios.c:2262
    push word [bp+016h]                       ; ff 76 16                    ; 0xc34ea
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc34ed
    push ax                                   ; 50                          ; 0xc34f0
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc34f1
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34f4
    xor ah, ah                                ; 30 e4                       ; 0xc34f7
    push ax                                   ; 50                          ; 0xc34f9
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc34fa
    mov bx, ax                                ; 89 c3                       ; 0xc34fd
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34ff
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3502
    xor ah, ah                                ; 30 e4                       ; 0xc3505
    xor dh, dh                                ; 30 f6                       ; 0xc3507
    mov si, dx                                ; 89 d6                       ; 0xc3509
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc350b
    mov dx, ax                                ; 89 c2                       ; 0xc350e
    mov ax, si                                ; 89 f0                       ; 0xc3510
    call 028d0h                               ; e8 bb f3                    ; 0xc3512
    jmp near 035f1h                           ; e9 d9 00                    ; 0xc3515 vgabios.c:2263
    mov bx, si                                ; 89 f3                       ; 0xc3518 vgabios.c:2265
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc351a
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc351d
    call 02972h                               ; e8 4f f4                    ; 0xc3520
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3523 vgabios.c:2266
    xor al, al                                ; 30 c0                       ; 0xc3526
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3528
    jmp near 032d1h                           ; e9 a4 fd                    ; 0xc352a
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc352d vgabios.c:2269
    je short 03554h                           ; 74 22                       ; 0xc3530
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3532
    je short 03546h                           ; 74 0f                       ; 0xc3535
    test ax, ax                               ; 85 c0                       ; 0xc3537
    jne short 03560h                          ; 75 25                       ; 0xc3539
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc353b vgabios.c:2272
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc353e
    call 02a97h                               ; e8 53 f5                    ; 0xc3541
    jmp short 03560h                          ; eb 1a                       ; 0xc3544 vgabios.c:2273
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3546 vgabios.c:2275
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3549
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc354c
    call 02aa9h                               ; e8 57 f5                    ; 0xc354f
    jmp short 03560h                          ; eb 0c                       ; 0xc3552 vgabios.c:2276
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3554 vgabios.c:2278
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3557
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc355a
    call 02e2ah                               ; e8 ca f8                    ; 0xc355d
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3560 vgabios.c:2285
    xor al, al                                ; 30 c0                       ; 0xc3563
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3565
    jmp near 032d1h                           ; e9 67 fd                    ; 0xc3567
    call 007bfh                               ; e8 52 d2                    ; 0xc356a vgabios.c:2290
    test ax, ax                               ; 85 c0                       ; 0xc356d
    je short 035e3h                           ; 74 72                       ; 0xc356f
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3571 vgabios.c:2291
    xor ah, ah                                ; 30 e4                       ; 0xc3574
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3576
    jnbe short 035e5h                         ; 77 6a                       ; 0xc3579
    push CS                                   ; 0e                          ; 0xc357b
    pop ES                                    ; 07                          ; 0xc357c
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc357d
    mov di, 03251h                            ; bf 51 32                    ; 0xc3580
    repne scasb                               ; f2 ae                       ; 0xc3583
    sal cx, 1                                 ; d1 e1                       ; 0xc3585
    mov di, cx                                ; 89 cf                       ; 0xc3587
    mov ax, word [cs:di+03258h]               ; 2e 8b 85 58 32              ; 0xc3589
    jmp ax                                    ; ff e0                       ; 0xc358e
    mov bx, si                                ; 89 f3                       ; 0xc3590 vgabios.c:2294
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3592
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3595
    call 037adh                               ; e8 12 02                    ; 0xc3598
    jmp short 035f1h                          ; eb 54                       ; 0xc359b vgabios.c:2295
    mov cx, si                                ; 89 f1                       ; 0xc359d vgabios.c:2297
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc359f
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35a2
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35a5
    call 038dch                               ; e8 31 03                    ; 0xc35a8
    jmp short 035f1h                          ; eb 44                       ; 0xc35ab vgabios.c:2298
    mov cx, si                                ; 89 f1                       ; 0xc35ad vgabios.c:2300
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc35af
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc35b2
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35b5
    call 03999h                               ; e8 de 03                    ; 0xc35b8
    jmp short 035f1h                          ; eb 34                       ; 0xc35bb vgabios.c:2301
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc35bd vgabios.c:2303
    push ax                                   ; 50                          ; 0xc35c0
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc35c1
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc35c4
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35c7
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35ca
    call 03b82h                               ; e8 b2 05                    ; 0xc35cd
    jmp short 035f1h                          ; eb 1f                       ; 0xc35d0 vgabios.c:2304
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc35d2 vgabios.c:2306
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc35d5
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc35d8
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35db
    call 03c0eh                               ; e8 2d 06                    ; 0xc35de
    jmp short 035f1h                          ; eb 0e                       ; 0xc35e1 vgabios.c:2307
    jmp short 035ech                          ; eb 07                       ; 0xc35e3
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc35e5 vgabios.c:2329
    jmp short 035f1h                          ; eb 05                       ; 0xc35ea vgabios.c:2332
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc35ec vgabios.c:2334
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc35f1 vgabios.c:2344
    pop di                                    ; 5f                          ; 0xc35f4
    pop si                                    ; 5e                          ; 0xc35f5
    pop bp                                    ; 5d                          ; 0xc35f6
    retn                                      ; c3                          ; 0xc35f7
  ; disGetNextSymbol 0xc35f8 LB 0x6f5 -> off=0x0 cb=000000000000001f uValue=00000000000c35f8 'dispi_set_xres'
dispi_set_xres:                              ; 0xc35f8 LB 0x1f
    push bp                                   ; 55                          ; 0xc35f8 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc35f9
    push bx                                   ; 53                          ; 0xc35fb
    push dx                                   ; 52                          ; 0xc35fc
    mov bx, ax                                ; 89 c3                       ; 0xc35fd
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc35ff vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3602
    call 00570h                               ; e8 68 cf                    ; 0xc3605
    mov ax, bx                                ; 89 d8                       ; 0xc3608 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc360a
    call 00570h                               ; e8 60 cf                    ; 0xc360d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3610 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3613
    pop bx                                    ; 5b                          ; 0xc3614
    pop bp                                    ; 5d                          ; 0xc3615
    retn                                      ; c3                          ; 0xc3616
  ; disGetNextSymbol 0xc3617 LB 0x6d6 -> off=0x0 cb=000000000000001f uValue=00000000000c3617 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3617 LB 0x1f
    push bp                                   ; 55                          ; 0xc3617 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3618
    push bx                                   ; 53                          ; 0xc361a
    push dx                                   ; 52                          ; 0xc361b
    mov bx, ax                                ; 89 c3                       ; 0xc361c
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc361e vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3621
    call 00570h                               ; e8 49 cf                    ; 0xc3624
    mov ax, bx                                ; 89 d8                       ; 0xc3627 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3629
    call 00570h                               ; e8 41 cf                    ; 0xc362c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc362f vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3632
    pop bx                                    ; 5b                          ; 0xc3633
    pop bp                                    ; 5d                          ; 0xc3634
    retn                                      ; c3                          ; 0xc3635
  ; disGetNextSymbol 0xc3636 LB 0x6b7 -> off=0x0 cb=0000000000000019 uValue=00000000000c3636 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3636 LB 0x19
    push bp                                   ; 55                          ; 0xc3636 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3637
    push dx                                   ; 52                          ; 0xc3639
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc363a vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc363d
    call 00570h                               ; e8 2d cf                    ; 0xc3640
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3643 vbe.c:121
    call 00577h                               ; e8 2e cf                    ; 0xc3646
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3649 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc364c
    pop bp                                    ; 5d                          ; 0xc364d
    retn                                      ; c3                          ; 0xc364e
  ; disGetNextSymbol 0xc364f LB 0x69e -> off=0x0 cb=000000000000001f uValue=00000000000c364f 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc364f LB 0x1f
    push bp                                   ; 55                          ; 0xc364f vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3650
    push bx                                   ; 53                          ; 0xc3652
    push dx                                   ; 52                          ; 0xc3653
    mov bx, ax                                ; 89 c3                       ; 0xc3654
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3656 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3659
    call 00570h                               ; e8 11 cf                    ; 0xc365c
    mov ax, bx                                ; 89 d8                       ; 0xc365f vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3661
    call 00570h                               ; e8 09 cf                    ; 0xc3664
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3667 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc366a
    pop bx                                    ; 5b                          ; 0xc366b
    pop bp                                    ; 5d                          ; 0xc366c
    retn                                      ; c3                          ; 0xc366d
  ; disGetNextSymbol 0xc366e LB 0x67f -> off=0x0 cb=0000000000000019 uValue=00000000000c366e 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc366e LB 0x19
    push bp                                   ; 55                          ; 0xc366e vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc366f
    push dx                                   ; 52                          ; 0xc3671
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3672 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3675
    call 00570h                               ; e8 f5 ce                    ; 0xc3678
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc367b vbe.c:136
    call 00577h                               ; e8 f6 ce                    ; 0xc367e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3681 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3684
    pop bp                                    ; 5d                          ; 0xc3685
    retn                                      ; c3                          ; 0xc3686
  ; disGetNextSymbol 0xc3687 LB 0x666 -> off=0x0 cb=000000000000001f uValue=00000000000c3687 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3687 LB 0x1f
    push bp                                   ; 55                          ; 0xc3687 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3688
    push bx                                   ; 53                          ; 0xc368a
    push dx                                   ; 52                          ; 0xc368b
    mov bx, ax                                ; 89 c3                       ; 0xc368c
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc368e vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3691
    call 00570h                               ; e8 d9 ce                    ; 0xc3694
    mov ax, bx                                ; 89 d8                       ; 0xc3697 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3699
    call 00570h                               ; e8 d1 ce                    ; 0xc369c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc369f vbe.c:146
    pop dx                                    ; 5a                          ; 0xc36a2
    pop bx                                    ; 5b                          ; 0xc36a3
    pop bp                                    ; 5d                          ; 0xc36a4
    retn                                      ; c3                          ; 0xc36a5
  ; disGetNextSymbol 0xc36a6 LB 0x647 -> off=0x0 cb=0000000000000019 uValue=00000000000c36a6 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc36a6 LB 0x19
    push bp                                   ; 55                          ; 0xc36a6 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc36a7
    push dx                                   ; 52                          ; 0xc36a9
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc36aa vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc36ad
    call 00570h                               ; e8 bd ce                    ; 0xc36b0
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc36b3 vbe.c:151
    call 00577h                               ; e8 be ce                    ; 0xc36b6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36b9 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc36bc
    pop bp                                    ; 5d                          ; 0xc36bd
    retn                                      ; c3                          ; 0xc36be
  ; disGetNextSymbol 0xc36bf LB 0x62e -> off=0x0 cb=0000000000000019 uValue=00000000000c36bf 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc36bf LB 0x19
    push bp                                   ; 55                          ; 0xc36bf vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc36c0
    push dx                                   ; 52                          ; 0xc36c2
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc36c3 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc36c6
    call 00570h                               ; e8 a4 ce                    ; 0xc36c9
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc36cc vbe.c:157
    call 00577h                               ; e8 a5 ce                    ; 0xc36cf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36d2 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc36d5
    pop bp                                    ; 5d                          ; 0xc36d6
    retn                                      ; c3                          ; 0xc36d7
  ; disGetNextSymbol 0xc36d8 LB 0x615 -> off=0x0 cb=0000000000000012 uValue=00000000000c36d8 'in_word'
in_word:                                     ; 0xc36d8 LB 0x12
    push bp                                   ; 55                          ; 0xc36d8 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc36d9
    push bx                                   ; 53                          ; 0xc36db
    mov bx, ax                                ; 89 c3                       ; 0xc36dc
    mov ax, dx                                ; 89 d0                       ; 0xc36de
    mov dx, bx                                ; 89 da                       ; 0xc36e0 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc36e2
    in ax, DX                                 ; ed                          ; 0xc36e3 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36e4 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc36e7
    pop bp                                    ; 5d                          ; 0xc36e8
    retn                                      ; c3                          ; 0xc36e9
  ; disGetNextSymbol 0xc36ea LB 0x603 -> off=0x0 cb=0000000000000014 uValue=00000000000c36ea 'in_byte'
in_byte:                                     ; 0xc36ea LB 0x14
    push bp                                   ; 55                          ; 0xc36ea vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc36eb
    push bx                                   ; 53                          ; 0xc36ed
    mov bx, ax                                ; 89 c3                       ; 0xc36ee
    mov ax, dx                                ; 89 d0                       ; 0xc36f0
    mov dx, bx                                ; 89 da                       ; 0xc36f2 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc36f4
    in AL, DX                                 ; ec                          ; 0xc36f5 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc36f6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36f8 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc36fb
    pop bp                                    ; 5d                          ; 0xc36fc
    retn                                      ; c3                          ; 0xc36fd
  ; disGetNextSymbol 0xc36fe LB 0x5ef -> off=0x0 cb=0000000000000014 uValue=00000000000c36fe 'dispi_get_id'
dispi_get_id:                                ; 0xc36fe LB 0x14
    push bp                                   ; 55                          ; 0xc36fe vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc36ff
    push dx                                   ; 52                          ; 0xc3701
    xor ax, ax                                ; 31 c0                       ; 0xc3702 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3704
    out DX, ax                                ; ef                          ; 0xc3707
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3708 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc370b
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc370c vbe.c:177
    pop dx                                    ; 5a                          ; 0xc370f
    pop bp                                    ; 5d                          ; 0xc3710
    retn                                      ; c3                          ; 0xc3711
  ; disGetNextSymbol 0xc3712 LB 0x5db -> off=0x0 cb=000000000000001a uValue=00000000000c3712 'dispi_set_id'
dispi_set_id:                                ; 0xc3712 LB 0x1a
    push bp                                   ; 55                          ; 0xc3712 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3713
    push bx                                   ; 53                          ; 0xc3715
    push dx                                   ; 52                          ; 0xc3716
    mov bx, ax                                ; 89 c3                       ; 0xc3717
    xor ax, ax                                ; 31 c0                       ; 0xc3719 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc371b
    out DX, ax                                ; ef                          ; 0xc371e
    mov ax, bx                                ; 89 d8                       ; 0xc371f vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3721
    out DX, ax                                ; ef                          ; 0xc3724
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3725 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3728
    pop bx                                    ; 5b                          ; 0xc3729
    pop bp                                    ; 5d                          ; 0xc372a
    retn                                      ; c3                          ; 0xc372b
  ; disGetNextSymbol 0xc372c LB 0x5c1 -> off=0x0 cb=000000000000002c uValue=00000000000c372c 'vbe_init'
vbe_init:                                    ; 0xc372c LB 0x2c
    push bp                                   ; 55                          ; 0xc372c vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc372d
    push bx                                   ; 53                          ; 0xc372f
    push dx                                   ; 52                          ; 0xc3730
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3731 vbe.c:190
    call 03712h                               ; e8 db ff                    ; 0xc3734
    call 036feh                               ; e8 c4 ff                    ; 0xc3737 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc373a
    jne short 03751h                          ; 75 12                       ; 0xc373d
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc373f vbe.c:193
    mov dx, 000b9h                            ; ba b9 00                    ; 0xc3742
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3745
    call 03196h                               ; e8 4b fa                    ; 0xc3748
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc374b vbe.c:194
    call 03712h                               ; e8 c1 ff                    ; 0xc374e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3751 vbe.c:199
    pop dx                                    ; 5a                          ; 0xc3754
    pop bx                                    ; 5b                          ; 0xc3755
    pop bp                                    ; 5d                          ; 0xc3756
    retn                                      ; c3                          ; 0xc3757
  ; disGetNextSymbol 0xc3758 LB 0x595 -> off=0x0 cb=0000000000000055 uValue=00000000000c3758 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3758 LB 0x55
    push bp                                   ; 55                          ; 0xc3758 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3759
    push bx                                   ; 53                          ; 0xc375b
    push cx                                   ; 51                          ; 0xc375c
    push si                                   ; 56                          ; 0xc375d
    push di                                   ; 57                          ; 0xc375e
    mov di, ax                                ; 89 c7                       ; 0xc375f
    mov si, dx                                ; 89 d6                       ; 0xc3761
    xor dx, dx                                ; 31 d2                       ; 0xc3763 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3765
    call 036d8h                               ; e8 6d ff                    ; 0xc3768
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc376b vbe.c:209
    jne short 037a2h                          ; 75 32                       ; 0xc376e
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3770 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3773 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3775
    call 036d8h                               ; e8 5d ff                    ; 0xc3778
    mov cx, ax                                ; 89 c1                       ; 0xc377b
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc377d vbe.c:219
    je short 037a2h                           ; 74 20                       ; 0xc3780
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3782 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3785
    call 036d8h                               ; e8 4d ff                    ; 0xc3788
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc378b
    cmp cx, di                                ; 39 f9                       ; 0xc378e vbe.c:223
    jne short 0379eh                          ; 75 0c                       ; 0xc3790
    test si, si                               ; 85 f6                       ; 0xc3792 vbe.c:225
    jne short 0379ah                          ; 75 04                       ; 0xc3794
    mov ax, bx                                ; 89 d8                       ; 0xc3796 vbe.c:226
    jmp short 037a4h                          ; eb 0a                       ; 0xc3798
    test AL, strict byte 080h                 ; a8 80                       ; 0xc379a vbe.c:227
    jne short 03796h                          ; 75 f8                       ; 0xc379c
    mov bx, dx                                ; 89 d3                       ; 0xc379e vbe.c:230
    jmp short 03775h                          ; eb d3                       ; 0xc37a0 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc37a2 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc37a4 vbe.c:239
    pop di                                    ; 5f                          ; 0xc37a7
    pop si                                    ; 5e                          ; 0xc37a8
    pop cx                                    ; 59                          ; 0xc37a9
    pop bx                                    ; 5b                          ; 0xc37aa
    pop bp                                    ; 5d                          ; 0xc37ab
    retn                                      ; c3                          ; 0xc37ac
  ; disGetNextSymbol 0xc37ad LB 0x540 -> off=0x0 cb=000000000000012f uValue=00000000000c37ad 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc37ad LB 0x12f
    push bp                                   ; 55                          ; 0xc37ad vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc37ae
    push cx                                   ; 51                          ; 0xc37b0
    push si                                   ; 56                          ; 0xc37b1
    push di                                   ; 57                          ; 0xc37b2
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc37b3
    mov si, ax                                ; 89 c6                       ; 0xc37b6
    mov di, dx                                ; 89 d7                       ; 0xc37b8
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc37ba
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc37bd vbe.c:275
    call 005b7h                               ; e8 f2 cd                    ; 0xc37c2 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc37c5
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc37c8 vbe.c:281
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc37cb
    xor dx, dx                                ; 31 d2                       ; 0xc37ce vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc37d0
    call 036d8h                               ; e8 02 ff                    ; 0xc37d3
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc37d6 vbe.c:285
    je short 037e5h                           ; 74 0a                       ; 0xc37d9
    push SS                                   ; 16                          ; 0xc37db vbe.c:287
    pop ES                                    ; 07                          ; 0xc37dc
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc37dd
    jmp near 038d4h                           ; e9 ef 00                    ; 0xc37e2 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc37e5 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc37e8 vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc37ed vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc37f0
    jne short 037ffh                          ; 75 07                       ; 0xc37f6
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc37f8
    je short 0380eh                           ; 74 0f                       ; 0xc37fd
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc37ff
    jne short 03813h                          ; 75 0c                       ; 0xc3805
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3807
    jne short 03813h                          ; 75 05                       ; 0xc380c
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc380e vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3813 vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3816
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc381b vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3821 vbe.c:324
    mov word [es:bx+006h], 07c6ch             ; 26 c7 47 06 6c 7c           ; 0xc3827 vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc382d
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc3831 vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3837 vbe.c:332
    mov word [es:bx+010h], di                 ; 26 89 7f 10                 ; 0xc383d vbe.c:336
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3841 vbe.c:337
    add ax, strict word 00022h                ; 05 22 00                    ; 0xc3844
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3847
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc384b vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc384e
    call 036d8h                               ; e8 84 fe                    ; 0xc3851
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3854
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3857
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc385b vbe.c:342
    je short 03885h                           ; 74 24                       ; 0xc385f
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3861 vbe.c:345
    mov word [es:bx+016h], 07c81h             ; 26 c7 47 16 81 7c           ; 0xc3867 vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc386d
    mov word [es:bx+01ah], 07c94h             ; 26 c7 47 1a 94 7c           ; 0xc3871 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3877
    mov word [es:bx+01eh], 07cb5h             ; 26 c7 47 1e b5 7c           ; 0xc387b vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3881
    mov dx, cx                                ; 89 ca                       ; 0xc3885 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3887
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc388a
    call 036eah                               ; e8 5a fe                    ; 0xc388d
    xor ah, ah                                ; 30 e4                       ; 0xc3890 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3892
    jnbe short 038b0h                         ; 77 19                       ; 0xc3895
    mov dx, cx                                ; 89 ca                       ; 0xc3897 vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3899
    call 036d8h                               ; e8 39 fe                    ; 0xc389c
    mov bx, ax                                ; 89 c3                       ; 0xc389f
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc38a1 vbe.c:362
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc38a4
    mov ax, di                                ; 89 f8                       ; 0xc38a7
    call 031b2h                               ; e8 06 f9                    ; 0xc38a9
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc38ac vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc38b0 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc38b3 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc38b5
    call 036d8h                               ; e8 1d fe                    ; 0xc38b8
    mov bx, ax                                ; 89 c3                       ; 0xc38bb
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc38bd vbe.c:368
    jne short 03885h                          ; 75 c3                       ; 0xc38c0
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc38c2 vbe.c:371
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc38c5
    mov ax, di                                ; 89 f8                       ; 0xc38c8
    call 031b2h                               ; e8 e5 f8                    ; 0xc38ca
    push SS                                   ; 16                          ; 0xc38cd vbe.c:372
    pop ES                                    ; 07                          ; 0xc38ce
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc38cf
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc38d4 vbe.c:373
    pop di                                    ; 5f                          ; 0xc38d7
    pop si                                    ; 5e                          ; 0xc38d8
    pop cx                                    ; 59                          ; 0xc38d9
    pop bp                                    ; 5d                          ; 0xc38da
    retn                                      ; c3                          ; 0xc38db
  ; disGetNextSymbol 0xc38dc LB 0x411 -> off=0x0 cb=00000000000000bd uValue=00000000000c38dc 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc38dc LB 0xbd
    push bp                                   ; 55                          ; 0xc38dc vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc38dd
    push si                                   ; 56                          ; 0xc38df
    push di                                   ; 57                          ; 0xc38e0
    push ax                                   ; 50                          ; 0xc38e1
    push ax                                   ; 50                          ; 0xc38e2
    push ax                                   ; 50                          ; 0xc38e3
    mov ax, dx                                ; 89 d0                       ; 0xc38e4
    mov si, bx                                ; 89 de                       ; 0xc38e6
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc38e8
    test dh, 040h                             ; f6 c6 40                    ; 0xc38eb vbe.c:396
    je short 038f5h                           ; 74 05                       ; 0xc38ee
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc38f0
    jmp short 038f7h                          ; eb 02                       ; 0xc38f3
    xor dx, dx                                ; 31 d2                       ; 0xc38f5
    and ah, 001h                              ; 80 e4 01                    ; 0xc38f7 vbe.c:397
    call 03758h                               ; e8 5b fe                    ; 0xc38fa vbe.c:399
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc38fd
    test ax, ax                               ; 85 c0                       ; 0xc3900 vbe.c:401
    je short 0393ah                           ; 74 36                       ; 0xc3902
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3904 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3907
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc3909
    mov es, si                                ; 8e c6                       ; 0xc390c
    cld                                       ; fc                          ; 0xc390e
    jcxz 03913h                               ; e3 02                       ; 0xc390f
    rep stosb                                 ; f3 aa                       ; 0xc3911
    xor cx, cx                                ; 31 c9                       ; 0xc3913 vbe.c:407
    jmp short 0391ch                          ; eb 05                       ; 0xc3915
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3917
    jnc short 0393ch                          ; 73 20                       ; 0xc391a
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc391c vbe.c:410
    inc dx                                    ; 42                          ; 0xc391f
    inc dx                                    ; 42                          ; 0xc3920
    add dx, cx                                ; 01 ca                       ; 0xc3921
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3923
    call 036eah                               ; e8 c1 fd                    ; 0xc3926
    mov bl, al                                ; 88 c3                       ; 0xc3929 vbe.c:411
    xor bh, bh                                ; 30 ff                       ; 0xc392b
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc392d
    add dx, cx                                ; 01 ca                       ; 0xc3930
    mov ax, si                                ; 89 f0                       ; 0xc3932
    call 03196h                               ; e8 5f f8                    ; 0xc3934
    inc cx                                    ; 41                          ; 0xc3937 vbe.c:412
    jmp short 03917h                          ; eb dd                       ; 0xc3938
    jmp short 03987h                          ; eb 4b                       ; 0xc393a
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc393c vbe.c:413
    inc dx                                    ; 42                          ; 0xc393f
    inc dx                                    ; 42                          ; 0xc3940
    mov ax, si                                ; 89 f0                       ; 0xc3941
    call 03188h                               ; e8 42 f8                    ; 0xc3943
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3946 vbe.c:414
    je short 03966h                           ; 74 1c                       ; 0xc3948
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc394a vbe.c:415
    add dx, strict byte 0000ch                ; 83 c2 0c                    ; 0xc394d
    mov bx, 00629h                            ; bb 29 06                    ; 0xc3950
    mov ax, si                                ; 89 f0                       ; 0xc3953
    call 031b2h                               ; e8 5a f8                    ; 0xc3955
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3958 vbe.c:417
    add dx, strict byte 0000eh                ; 83 c2 0e                    ; 0xc395b
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc395e
    mov ax, si                                ; 89 f0                       ; 0xc3961
    call 031b2h                               ; e8 4c f8                    ; 0xc3963
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3966 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3969
    call 00570h                               ; e8 01 cc                    ; 0xc396c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc396f vbe.c:421
    call 00577h                               ; e8 02 cc                    ; 0xc3972
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3975
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc3978
    mov bx, ax                                ; 89 c3                       ; 0xc397b
    mov ax, si                                ; 89 f0                       ; 0xc397d
    call 031b2h                               ; e8 30 f8                    ; 0xc397f
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3982 vbe.c:423
    jmp short 0398ah                          ; eb 03                       ; 0xc3985 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3987 vbe.c:428
    push SS                                   ; 16                          ; 0xc398a vbe.c:431
    pop ES                                    ; 07                          ; 0xc398b
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc398c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc398f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3992 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3995
    pop si                                    ; 5e                          ; 0xc3996
    pop bp                                    ; 5d                          ; 0xc3997
    retn                                      ; c3                          ; 0xc3998
  ; disGetNextSymbol 0xc3999 LB 0x354 -> off=0x0 cb=00000000000000eb uValue=00000000000c3999 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3999 LB 0xeb
    push bp                                   ; 55                          ; 0xc3999 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc399a
    push si                                   ; 56                          ; 0xc399c
    push di                                   ; 57                          ; 0xc399d
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc399e
    mov si, ax                                ; 89 c6                       ; 0xc39a1
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc39a3
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc39a6 vbe.c:452
    je short 039b1h                           ; 74 05                       ; 0xc39aa
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc39ac
    jmp short 039b3h                          ; eb 02                       ; 0xc39af
    xor ax, ax                                ; 31 c0                       ; 0xc39b1
    mov dx, ax                                ; 89 c2                       ; 0xc39b3
    test ax, ax                               ; 85 c0                       ; 0xc39b5 vbe.c:453
    je short 039bch                           ; 74 03                       ; 0xc39b7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc39b9
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc39bc
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc39bf vbe.c:454
    je short 039cah                           ; 74 05                       ; 0xc39c3
    mov ax, 00080h                            ; b8 80 00                    ; 0xc39c5
    jmp short 039cch                          ; eb 02                       ; 0xc39c8
    xor ax, ax                                ; 31 c0                       ; 0xc39ca
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc39cc
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc39cf vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc39d3 vbe.c:459
    jnc short 039edh                          ; 73 13                       ; 0xc39d8
    xor ax, ax                                ; 31 c0                       ; 0xc39da vbe.c:463
    call 005ddh                               ; e8 fe cb                    ; 0xc39dc
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc39df vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc39e2
    call 0101ch                               ; e8 35 d6                    ; 0xc39e4
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc39e7 vbe.c:468
    jmp near 03a7ah                           ; e9 8d 00                    ; 0xc39ea vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc39ed vbe.c:472
    call 03758h                               ; e8 65 fd                    ; 0xc39f0
    mov bx, ax                                ; 89 c3                       ; 0xc39f3
    test ax, ax                               ; 85 c0                       ; 0xc39f5 vbe.c:474
    jne short 039fch                          ; 75 03                       ; 0xc39f7
    jmp near 03a77h                           ; e9 7b 00                    ; 0xc39f9
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc39fc vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc39ff
    call 036d8h                               ; e8 d3 fc                    ; 0xc3a02
    mov cx, ax                                ; 89 c1                       ; 0xc3a05
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3a07 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a0a
    call 036d8h                               ; e8 c8 fc                    ; 0xc3a0d
    mov di, ax                                ; 89 c7                       ; 0xc3a10
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3a12 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a15
    call 036eah                               ; e8 cf fc                    ; 0xc3a18
    mov bl, al                                ; 88 c3                       ; 0xc3a1b
    mov dl, al                                ; 88 c2                       ; 0xc3a1d
    xor ax, ax                                ; 31 c0                       ; 0xc3a1f vbe.c:489
    call 005ddh                               ; e8 b9 cb                    ; 0xc3a21
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3a24 vbe.c:491
    jne short 03a2fh                          ; 75 06                       ; 0xc3a27
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3a29 vbe.c:493
    call 0101ch                               ; e8 ed d5                    ; 0xc3a2c
    mov al, dl                                ; 88 d0                       ; 0xc3a2f vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3a31
    call 0364fh                               ; e8 19 fc                    ; 0xc3a33
    mov ax, cx                                ; 89 c8                       ; 0xc3a36 vbe.c:497
    call 035f8h                               ; e8 bd fb                    ; 0xc3a38
    mov ax, di                                ; 89 f8                       ; 0xc3a3b vbe.c:498
    call 03617h                               ; e8 d7 fb                    ; 0xc3a3d
    xor ax, ax                                ; 31 c0                       ; 0xc3a40 vbe.c:499
    call 00603h                               ; e8 be cb                    ; 0xc3a42
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3a45 vbe.c:500
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3a48
    xor ah, ah                                ; 30 e4                       ; 0xc3a4a
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3a4c
    or al, dl                                 ; 08 d0                       ; 0xc3a4f
    call 005ddh                               ; e8 89 cb                    ; 0xc3a51
    call 006d2h                               ; e8 7b cc                    ; 0xc3a54 vbe.c:501
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc3a57 vbe.c:503
    mov dx, 000bah                            ; ba ba 00                    ; 0xc3a5a
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3a5d
    call 031b2h                               ; e8 4f f7                    ; 0xc3a60
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc3a63 vbe.c:504
    or bl, 060h                               ; 80 cb 60                    ; 0xc3a66
    xor bh, bh                                ; 30 ff                       ; 0xc3a69
    mov dx, 00087h                            ; ba 87 00                    ; 0xc3a6b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3a6e
    call 03196h                               ; e8 22 f7                    ; 0xc3a71
    jmp near 039e7h                           ; e9 70 ff                    ; 0xc3a74
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3a77 vbe.c:513
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc3a7a vbe.c:517
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3a7d vbe.c:518
    pop di                                    ; 5f                          ; 0xc3a80
    pop si                                    ; 5e                          ; 0xc3a81
    pop bp                                    ; 5d                          ; 0xc3a82
    retn                                      ; c3                          ; 0xc3a83
  ; disGetNextSymbol 0xc3a84 LB 0x269 -> off=0x0 cb=0000000000000008 uValue=00000000000c3a84 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3a84 LB 0x8
    push bp                                   ; 55                          ; 0xc3a84 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3a85
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3a87 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3a8a
    retn                                      ; c3                          ; 0xc3a8b
  ; disGetNextSymbol 0xc3a8c LB 0x261 -> off=0x0 cb=000000000000005b uValue=00000000000c3a8c 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3a8c LB 0x5b
    push bp                                   ; 55                          ; 0xc3a8c vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3a8d
    push bx                                   ; 53                          ; 0xc3a8f
    push cx                                   ; 51                          ; 0xc3a90
    push si                                   ; 56                          ; 0xc3a91
    push di                                   ; 57                          ; 0xc3a92
    push ax                                   ; 50                          ; 0xc3a93
    mov di, ax                                ; 89 c7                       ; 0xc3a94
    mov cx, dx                                ; 89 d1                       ; 0xc3a96
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3a98 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3a9b
    out DX, ax                                ; ef                          ; 0xc3a9e
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3a9f vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3aa2
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3aa3
    mov bx, ax                                ; 89 c3                       ; 0xc3aa6 vbe.c:531
    mov dx, cx                                ; 89 ca                       ; 0xc3aa8
    mov ax, di                                ; 89 f8                       ; 0xc3aaa
    call 031b2h                               ; e8 03 f7                    ; 0xc3aac
    inc cx                                    ; 41                          ; 0xc3aaf vbe.c:532
    inc cx                                    ; 41                          ; 0xc3ab0
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc3ab1 vbe.c:533
    je short 03adeh                           ; 74 27                       ; 0xc3ab5
    mov si, strict word 00001h                ; be 01 00                    ; 0xc3ab7 vbe.c:535
    jmp short 03ac1h                          ; eb 05                       ; 0xc3aba
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3abc
    jnbe short 03adeh                         ; 77 1d                       ; 0xc3abf
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc3ac1 vbe.c:536
    je short 03adbh                           ; 74 15                       ; 0xc3ac4
    mov ax, si                                ; 89 f0                       ; 0xc3ac6 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ac8
    out DX, ax                                ; ef                          ; 0xc3acb
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3acc vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3acf
    mov bx, ax                                ; 89 c3                       ; 0xc3ad0
    mov dx, cx                                ; 89 ca                       ; 0xc3ad2
    mov ax, di                                ; 89 f8                       ; 0xc3ad4
    call 031b2h                               ; e8 d9 f6                    ; 0xc3ad6
    inc cx                                    ; 41                          ; 0xc3ad9 vbe.c:539
    inc cx                                    ; 41                          ; 0xc3ada
    inc si                                    ; 46                          ; 0xc3adb vbe.c:541
    jmp short 03abch                          ; eb de                       ; 0xc3adc
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3ade vbe.c:542
    pop di                                    ; 5f                          ; 0xc3ae1
    pop si                                    ; 5e                          ; 0xc3ae2
    pop cx                                    ; 59                          ; 0xc3ae3
    pop bx                                    ; 5b                          ; 0xc3ae4
    pop bp                                    ; 5d                          ; 0xc3ae5
    retn                                      ; c3                          ; 0xc3ae6
  ; disGetNextSymbol 0xc3ae7 LB 0x206 -> off=0x0 cb=000000000000009b uValue=00000000000c3ae7 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3ae7 LB 0x9b
    push bp                                   ; 55                          ; 0xc3ae7 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3ae8
    push bx                                   ; 53                          ; 0xc3aea
    push cx                                   ; 51                          ; 0xc3aeb
    push si                                   ; 56                          ; 0xc3aec
    push ax                                   ; 50                          ; 0xc3aed
    mov cx, ax                                ; 89 c1                       ; 0xc3aee
    mov bx, dx                                ; 89 d3                       ; 0xc3af0
    call 031a4h                               ; e8 af f6                    ; 0xc3af2 vbe.c:549
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3af5
    inc bx                                    ; 43                          ; 0xc3af8 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3af9
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3afa vbe.c:552
    jne short 03b10h                          ; 75 10                       ; 0xc3afe
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3b00 vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b03
    out DX, ax                                ; ef                          ; 0xc3b06
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3b07 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b0a
    out DX, ax                                ; ef                          ; 0xc3b0d
    jmp short 03b7ah                          ; eb 6a                       ; 0xc3b0e vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3b10 vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b13
    out DX, ax                                ; ef                          ; 0xc3b16
    mov dx, bx                                ; 89 da                       ; 0xc3b17 vbe.c:557
    mov ax, cx                                ; 89 c8                       ; 0xc3b19
    call 031a4h                               ; e8 86 f6                    ; 0xc3b1b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b1e
    out DX, ax                                ; ef                          ; 0xc3b21
    inc bx                                    ; 43                          ; 0xc3b22 vbe.c:558
    inc bx                                    ; 43                          ; 0xc3b23
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b24
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b27
    out DX, ax                                ; ef                          ; 0xc3b2a
    mov dx, bx                                ; 89 da                       ; 0xc3b2b vbe.c:560
    mov ax, cx                                ; 89 c8                       ; 0xc3b2d
    call 031a4h                               ; e8 72 f6                    ; 0xc3b2f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b32
    out DX, ax                                ; ef                          ; 0xc3b35
    inc bx                                    ; 43                          ; 0xc3b36 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3b37
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b38
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b3b
    out DX, ax                                ; ef                          ; 0xc3b3e
    mov dx, bx                                ; 89 da                       ; 0xc3b3f vbe.c:563
    mov ax, cx                                ; 89 c8                       ; 0xc3b41
    call 031a4h                               ; e8 5e f6                    ; 0xc3b43
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b46
    out DX, ax                                ; ef                          ; 0xc3b49
    inc bx                                    ; 43                          ; 0xc3b4a vbe.c:564
    inc bx                                    ; 43                          ; 0xc3b4b
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3b4c
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b4f
    out DX, ax                                ; ef                          ; 0xc3b52
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3b53 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b56
    out DX, ax                                ; ef                          ; 0xc3b59
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3b5a vbe.c:568
    jmp short 03b64h                          ; eb 05                       ; 0xc3b5d
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3b5f
    jnbe short 03b7ah                         ; 77 16                       ; 0xc3b62
    mov ax, si                                ; 89 f0                       ; 0xc3b64 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b66
    out DX, ax                                ; ef                          ; 0xc3b69
    mov dx, bx                                ; 89 da                       ; 0xc3b6a vbe.c:570
    mov ax, cx                                ; 89 c8                       ; 0xc3b6c
    call 031a4h                               ; e8 33 f6                    ; 0xc3b6e
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b71
    out DX, ax                                ; ef                          ; 0xc3b74
    inc bx                                    ; 43                          ; 0xc3b75 vbe.c:571
    inc bx                                    ; 43                          ; 0xc3b76
    inc si                                    ; 46                          ; 0xc3b77 vbe.c:572
    jmp short 03b5fh                          ; eb e5                       ; 0xc3b78
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3b7a vbe.c:574
    pop si                                    ; 5e                          ; 0xc3b7d
    pop cx                                    ; 59                          ; 0xc3b7e
    pop bx                                    ; 5b                          ; 0xc3b7f
    pop bp                                    ; 5d                          ; 0xc3b80
    retn                                      ; c3                          ; 0xc3b81
  ; disGetNextSymbol 0xc3b82 LB 0x16b -> off=0x0 cb=000000000000008c uValue=00000000000c3b82 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc3b82 LB 0x8c
    push bp                                   ; 55                          ; 0xc3b82 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc3b83
    push si                                   ; 56                          ; 0xc3b85
    push di                                   ; 57                          ; 0xc3b86
    push ax                                   ; 50                          ; 0xc3b87
    mov si, ax                                ; 89 c6                       ; 0xc3b88
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc3b8a
    mov ax, bx                                ; 89 d8                       ; 0xc3b8d
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc3b8f
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3b92 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc3b95 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3b97
    je short 03be1h                           ; 74 45                       ; 0xc3b9a
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3b9c
    je short 03bc5h                           ; 74 24                       ; 0xc3b9f
    test ax, ax                               ; 85 c0                       ; 0xc3ba1
    jne short 03bfdh                          ; 75 58                       ; 0xc3ba3
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3ba5 vbe.c:598
    call 02a74h                               ; e8 c9 ee                    ; 0xc3ba8
    mov cx, ax                                ; 89 c1                       ; 0xc3bab
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3bad vbe.c:602
    je short 03bb8h                           ; 74 05                       ; 0xc3bb1
    call 03a84h                               ; e8 ce fe                    ; 0xc3bb3 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc3bb6
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3bb8 vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3bbb
    push SS                                   ; 16                          ; 0xc3bbe
    pop ES                                    ; 07                          ; 0xc3bbf
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3bc0
    jmp short 03c00h                          ; eb 3b                       ; 0xc3bc3 vbe.c:605
    push SS                                   ; 16                          ; 0xc3bc5 vbe.c:607
    pop ES                                    ; 07                          ; 0xc3bc6
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3bc7
    mov dx, cx                                ; 89 ca                       ; 0xc3bca vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3bcc
    call 02aa9h                               ; e8 d7 ee                    ; 0xc3bcf
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3bd2 vbe.c:612
    je short 03c00h                           ; 74 28                       ; 0xc3bd6
    mov dx, ax                                ; 89 c2                       ; 0xc3bd8 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc3bda
    call 03a8ch                               ; e8 ad fe                    ; 0xc3bdc
    jmp short 03c00h                          ; eb 1f                       ; 0xc3bdf vbe.c:614
    push SS                                   ; 16                          ; 0xc3be1 vbe.c:616
    pop ES                                    ; 07                          ; 0xc3be2
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3be3
    mov dx, cx                                ; 89 ca                       ; 0xc3be6 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3be8
    call 02e2ah                               ; e8 3c f2                    ; 0xc3beb
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3bee vbe.c:621
    je short 03c00h                           ; 74 0c                       ; 0xc3bf2
    mov dx, ax                                ; 89 c2                       ; 0xc3bf4 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc3bf6
    call 03ae7h                               ; e8 ec fe                    ; 0xc3bf8
    jmp short 03c00h                          ; eb 03                       ; 0xc3bfb vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc3bfd vbe.c:626
    push SS                                   ; 16                          ; 0xc3c00 vbe.c:629
    pop ES                                    ; 07                          ; 0xc3c01
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc3c02
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c05 vbe.c:630
    pop di                                    ; 5f                          ; 0xc3c08
    pop si                                    ; 5e                          ; 0xc3c09
    pop bp                                    ; 5d                          ; 0xc3c0a
    retn 00002h                               ; c2 02 00                    ; 0xc3c0b
  ; disGetNextSymbol 0xc3c0e LB 0xdf -> off=0x0 cb=00000000000000df uValue=00000000000c3c0e 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc3c0e LB 0xdf
    push bp                                   ; 55                          ; 0xc3c0e vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc3c0f
    push si                                   ; 56                          ; 0xc3c11
    push di                                   ; 57                          ; 0xc3c12
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc3c13
    push ax                                   ; 50                          ; 0xc3c16
    mov di, dx                                ; 89 d7                       ; 0xc3c17
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc3c19
    mov si, cx                                ; 89 ce                       ; 0xc3c1c
    call 0366eh                               ; e8 4d fa                    ; 0xc3c1e vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3c21 vbe.c:661
    jne short 03c2ah                          ; 75 05                       ; 0xc3c23
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc3c25
    jmp short 03c2eh                          ; eb 04                       ; 0xc3c28
    xor ah, ah                                ; 30 e4                       ; 0xc3c2a
    mov bx, ax                                ; 89 c3                       ; 0xc3c2c
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc3c2e
    call 036a6h                               ; e8 72 fa                    ; 0xc3c31 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3c34
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc3c37 vbe.c:663
    push SS                                   ; 16                          ; 0xc3c3c vbe.c:664
    pop ES                                    ; 07                          ; 0xc3c3d
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3c3e
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3c41
    mov cl, byte [es:di]                      ; 26 8a 0d                    ; 0xc3c44 vbe.c:665
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc3c47 vbe.c:669
    je short 03c58h                           ; 74 0c                       ; 0xc3c4a
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc3c4c
    je short 03c7eh                           ; 74 2d                       ; 0xc3c4f
    test cl, cl                               ; 84 c9                       ; 0xc3c51
    je short 03c79h                           ; 74 24                       ; 0xc3c53
    jmp near 03cd6h                           ; e9 7e 00                    ; 0xc3c55
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3c58 vbe.c:671
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3c5b
    jne short 03c64h                          ; 75 05                       ; 0xc3c5d
    sal bx, 003h                              ; c1 e3 03                    ; 0xc3c5f vbe.c:672
    jmp short 03c79h                          ; eb 15                       ; 0xc3c62 vbe.c:673
    xor ah, ah                                ; 30 e4                       ; 0xc3c64 vbe.c:674
    cwd                                       ; 99                          ; 0xc3c66
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3c67
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3c6a
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3c6c
    mov cx, ax                                ; 89 c1                       ; 0xc3c6f
    mov ax, bx                                ; 89 d8                       ; 0xc3c71
    xor dx, dx                                ; 31 d2                       ; 0xc3c73
    div cx                                    ; f7 f1                       ; 0xc3c75
    mov bx, ax                                ; 89 c3                       ; 0xc3c77
    mov ax, bx                                ; 89 d8                       ; 0xc3c79 vbe.c:677
    call 03687h                               ; e8 09 fa                    ; 0xc3c7b
    call 036a6h                               ; e8 25 fa                    ; 0xc3c7e vbe.c:680
    mov cx, ax                                ; 89 c1                       ; 0xc3c81
    push SS                                   ; 16                          ; 0xc3c83 vbe.c:681
    pop ES                                    ; 07                          ; 0xc3c84
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3c85
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3c88
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3c8b vbe.c:682
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3c8e
    jne short 03c99h                          ; 75 07                       ; 0xc3c90
    mov bx, cx                                ; 89 cb                       ; 0xc3c92 vbe.c:683
    shr bx, 003h                              ; c1 eb 03                    ; 0xc3c94
    jmp short 03cach                          ; eb 13                       ; 0xc3c97 vbe.c:684
    xor ah, ah                                ; 30 e4                       ; 0xc3c99 vbe.c:685
    cwd                                       ; 99                          ; 0xc3c9b
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3c9c
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3c9f
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3ca1
    mov bx, ax                                ; 89 c3                       ; 0xc3ca4
    mov ax, cx                                ; 89 c8                       ; 0xc3ca6
    mul bx                                    ; f7 e3                       ; 0xc3ca8
    mov bx, ax                                ; 89 c3                       ; 0xc3caa
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc3cac vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc3caf
    push SS                                   ; 16                          ; 0xc3cb2 vbe.c:687
    pop ES                                    ; 07                          ; 0xc3cb3
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc3cb4
    call 036bfh                               ; e8 05 fa                    ; 0xc3cb7 vbe.c:688
    push SS                                   ; 16                          ; 0xc3cba
    pop ES                                    ; 07                          ; 0xc3cbb
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3cbc
    call 03636h                               ; e8 74 f9                    ; 0xc3cbf vbe.c:689
    push SS                                   ; 16                          ; 0xc3cc2
    pop ES                                    ; 07                          ; 0xc3cc3
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc3cc4
    jbe short 03cdbh                          ; 76 12                       ; 0xc3cc7
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3cc9 vbe.c:690
    call 03687h                               ; e8 b8 f9                    ; 0xc3ccc
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc3ccf vbe.c:691
    jmp short 03cdbh                          ; eb 05                       ; 0xc3cd4 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc3cd6 vbe.c:696
    push SS                                   ; 16                          ; 0xc3cdb vbe.c:699
    pop ES                                    ; 07                          ; 0xc3cdc
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc3cdd
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc3ce0
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3ce3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ce6 vbe.c:700
    pop di                                    ; 5f                          ; 0xc3ce9
    pop si                                    ; 5e                          ; 0xc3cea
    pop bp                                    ; 5d                          ; 0xc3ceb
    retn                                      ; c3                          ; 0xc3cec

  ; Padding 0x713 bytes at 0xc3ced
  times 1811 db 0

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
    db  069h, 06fh, 073h, 032h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h
    db  06fh, 073h, 032h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0fch
