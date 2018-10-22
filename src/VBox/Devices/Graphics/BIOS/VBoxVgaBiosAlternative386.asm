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





section VGAROM progbits vstart=0x0 align=1 ; size=0x90a class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x90a -> off=0x22 cb=000000000000054e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 062h, 00ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    call 008f3h                               ; e8 19 08                    ; 0xc00d7 vgarom.asm:189
    jmp short 000eah                          ; eb 0e                       ; 0xc00da vgarom.asm:190
    push ES                                   ; 06                          ; 0xc00dc vgarom.asm:194
    push DS                                   ; 1e                          ; 0xc00dd vgarom.asm:195
    pushaw                                    ; 60                          ; 0xc00de vgarom.asm:97
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc00df vgarom.asm:199
    mov ds, bx                                ; 8e db                       ; 0xc00e2 vgarom.asm:200
    call 03037h                               ; e8 50 2f                    ; 0xc00e4 vgarom.asm:201
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
  ; disGetNextSymbol 0xc0570 LB 0x39a -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x393 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05b7 LB 0x353 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc05dd LB 0x32d -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc0603 LB 0x307 -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc0629 LB 0x2e1 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06d2 LB 0x238 -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07bf LB 0x14b -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07d2 LB 0x138 -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc07f7 LB 0x113 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc0824 LB 0xe6 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0858 LB 0xb2 -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc088f LB 0x7b -> off=0x0 cb=0000000000000064 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x64
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008cah                           ; 74 32                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008efh                          ; 76 52                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008ebh                          ; 75 49                       ; 0xc08a0 vberom.asm:690
    pushad                                    ; 66 60                       ; 0xc08a2 vberom.asm:131
    push DS                                   ; 1e                          ; 0xc08a4 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08a5 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08a6 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08a7 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08a9 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08ac vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08ad vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08ae vberom.asm:703
    lodsd                                     ; 66 ad                       ; 0xc08b0 vberom.asm:706
    ror eax, 010h                             ; 66 c1 c8 10                 ; 0xc08b2 vberom.asm:707
    out DX, AL                                ; ee                          ; 0xc08b6 vberom.asm:708
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08b7 vberom.asm:709
    out DX, AL                                ; ee                          ; 0xc08bb vberom.asm:710
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08bc vberom.asm:711
    out DX, AL                                ; ee                          ; 0xc08c0 vberom.asm:712
    loop 008b0h                               ; e2 ed                       ; 0xc08c1 vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08c3 vberom.asm:724
    popad                                     ; 66 61                       ; 0xc08c4 vberom.asm:150
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08c6 vberom.asm:727
    retn                                      ; c3                          ; 0xc08c9 vberom.asm:728
    pushad                                    ; 66 60                       ; 0xc08ca vberom.asm:131
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08cc vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08ce vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08d1 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08d2 vberom.asm:735
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0                  ; 0xc08d5 vberom.asm:738
    in AL, DX                                 ; ec                          ; 0xc08d8 vberom.asm:739
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08d9 vberom.asm:740
    in AL, DX                                 ; ec                          ; 0xc08dd vberom.asm:741
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08de vberom.asm:742
    in AL, DX                                 ; ec                          ; 0xc08e2 vberom.asm:743
    stosd                                     ; 66 ab                       ; 0xc08e3 vberom.asm:744
    loop 008d5h                               ; e2 ee                       ; 0xc08e5 vberom.asm:757
    popad                                     ; 66 61                       ; 0xc08e7 vberom.asm:150
    jmp short 008c6h                          ; eb db                       ; 0xc08e9 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08eb vberom.asm:762
    retn                                      ; c3                          ; 0xc08ee vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08ef vberom.asm:765
    retn                                      ; c3                          ; 0xc08f2 vberom.asm:766
  ; disGetNextSymbol 0xc08f3 LB 0x17 -> off=0x0 cb=0000000000000017 uValue=00000000000c08f3 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08f3 LB 0x17
    test bl, bl                               ; 84 db                       ; 0xc08f3 vberom.asm:780
    jne short 00906h                          ; 75 0f                       ; 0xc08f5 vberom.asm:781
    mov di, 0c000h                            ; bf 00 c0                    ; 0xc08f7 vberom.asm:782
    mov es, di                                ; 8e c7                       ; 0xc08fa vberom.asm:783
    mov di, 04400h                            ; bf 00 44                    ; 0xc08fc vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08ff vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0902 vberom.asm:786
    retn                                      ; c3                          ; 0xc0905 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0906 vberom.asm:789
    retn                                      ; c3                          ; 0xc0909 vberom.asm:790

  ; Padding 0xf6 bytes at 0xc090a
  times 246 db 0

section _TEXT progbits vstart=0xa00 align=1 ; size=0x30d9 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0a00 LB 0x30d9 -> off=0x0 cb=000000000000001a uValue=00000000000c0a00 'set_int_vector'
set_int_vector:                              ; 0xc0a00 LB 0x1a
    push bx                                   ; 53                          ; 0xc0a00 vgabios.c:85
    push bp                                   ; 55                          ; 0xc0a01
    mov bp, sp                                ; 89 e5                       ; 0xc0a02
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0a04 vgabios.c:89
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0a07
    xor ax, ax                                ; 31 c0                       ; 0xc0a0a
    mov es, ax                                ; 8e c0                       ; 0xc0a0c
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a0e
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0a11
    pop bp                                    ; 5d                          ; 0xc0a17 vgabios.c:90
    pop bx                                    ; 5b                          ; 0xc0a18
    retn                                      ; c3                          ; 0xc0a19
  ; disGetNextSymbol 0xc0a1a LB 0x30bf -> off=0x0 cb=000000000000001c uValue=00000000000c0a1a 'init_vga_card'
init_vga_card:                               ; 0xc0a1a LB 0x1c
    push bp                                   ; 55                          ; 0xc0a1a vgabios.c:141
    mov bp, sp                                ; 89 e5                       ; 0xc0a1b
    push dx                                   ; 52                          ; 0xc0a1d
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a1e vgabios.c:144
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a20
    out DX, AL                                ; ee                          ; 0xc0a23
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a24 vgabios.c:147
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a26
    out DX, AL                                ; ee                          ; 0xc0a29
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a2a vgabios.c:148
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a2c
    out DX, AL                                ; ee                          ; 0xc0a2f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a30 vgabios.c:153
    pop dx                                    ; 5a                          ; 0xc0a33
    pop bp                                    ; 5d                          ; 0xc0a34
    retn                                      ; c3                          ; 0xc0a35
  ; disGetNextSymbol 0xc0a36 LB 0x30a3 -> off=0x0 cb=0000000000000032 uValue=00000000000c0a36 'init_bios_area'
init_bios_area:                              ; 0xc0a36 LB 0x32
    push bx                                   ; 53                          ; 0xc0a36 vgabios.c:162
    push bp                                   ; 55                          ; 0xc0a37
    mov bp, sp                                ; 89 e5                       ; 0xc0a38
    xor bx, bx                                ; 31 db                       ; 0xc0a3a vgabios.c:166
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a3c
    mov es, ax                                ; 8e c0                       ; 0xc0a3f
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a41 vgabios.c:169
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a45
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a47
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a49
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a4d vgabios.c:173
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a53 vgabios.c:175
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a5a vgabios.c:179
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a60 vgabios.c:181
    pop bp                                    ; 5d                          ; 0xc0a65 vgabios.c:182
    pop bx                                    ; 5b                          ; 0xc0a66
    retn                                      ; c3                          ; 0xc0a67
  ; disGetNextSymbol 0xc0a68 LB 0x3071 -> off=0x0 cb=0000000000000020 uValue=00000000000c0a68 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a68 LB 0x20
    push bp                                   ; 55                          ; 0xc0a68 vgabios.c:222
    mov bp, sp                                ; 89 e5                       ; 0xc0a69
    call 00a1ah                               ; e8 ac ff                    ; 0xc0a6b vgabios.c:224
    call 00a36h                               ; e8 c5 ff                    ; 0xc0a6e vgabios.c:225
    call 03535h                               ; e8 c1 2a                    ; 0xc0a71 vgabios.c:227
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc0a74 vgabios.c:229
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a77
    call 00a00h                               ; e8 83 ff                    ; 0xc0a7a
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a7d vgabios.c:255
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a80
    int 010h                                  ; cd 10                       ; 0xc0a82
    mov sp, bp                                ; 89 ec                       ; 0xc0a84 vgabios.c:258
    pop bp                                    ; 5d                          ; 0xc0a86
    retf                                      ; cb                          ; 0xc0a87
  ; disGetNextSymbol 0xc0a88 LB 0x3051 -> off=0x0 cb=0000000000000043 uValue=00000000000c0a88 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a88 LB 0x43
    push bp                                   ; 55                          ; 0xc0a88 vgabios.c:327
    mov bp, sp                                ; 89 e5                       ; 0xc0a89
    push cx                                   ; 51                          ; 0xc0a8b
    push si                                   ; 56                          ; 0xc0a8c
    mov cl, al                                ; 88 c1                       ; 0xc0a8d
    mov si, dx                                ; 89 d6                       ; 0xc0a8f
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a91 vgabios.c:329
    jbe short 00aa3h                          ; 76 0e                       ; 0xc0a93
    push SS                                   ; 16                          ; 0xc0a95 vgabios.c:330
    pop ES                                    ; 07                          ; 0xc0a96
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a97
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a9c vgabios.c:331
    jmp short 00ac4h                          ; eb 21                       ; 0xc0aa1 vgabios.c:332
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0aa3 vgabios.c:334
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0aa6
    call 02f73h                               ; e8 c7 24                    ; 0xc0aa9
    push SS                                   ; 16                          ; 0xc0aac
    pop ES                                    ; 07                          ; 0xc0aad
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0aae
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc0ab1 vgabios.c:335
    add dx, dx                                ; 01 d2                       ; 0xc0ab4
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0ab6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ab9
    call 02f73h                               ; e8 b4 24                    ; 0xc0abc
    push SS                                   ; 16                          ; 0xc0abf
    pop ES                                    ; 07                          ; 0xc0ac0
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ac1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ac4 vgabios.c:337
    pop si                                    ; 5e                          ; 0xc0ac7
    pop cx                                    ; 59                          ; 0xc0ac8
    pop bp                                    ; 5d                          ; 0xc0ac9
    retn                                      ; c3                          ; 0xc0aca
  ; disGetNextSymbol 0xc0acb LB 0x300e -> off=0x0 cb=0000000000000098 uValue=00000000000c0acb 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0acb LB 0x98
    push bp                                   ; 55                          ; 0xc0acb vgabios.c:340
    mov bp, sp                                ; 89 e5                       ; 0xc0acc
    push bx                                   ; 53                          ; 0xc0ace
    push cx                                   ; 51                          ; 0xc0acf
    push si                                   ; 56                          ; 0xc0ad0
    push di                                   ; 57                          ; 0xc0ad1
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0ad2
    mov cl, al                                ; 88 c1                       ; 0xc0ad5
    mov si, dx                                ; 89 d6                       ; 0xc0ad7
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0ad9 vgabios.c:347
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0adc
    call 02f57h                               ; e8 75 24                    ; 0xc0adf
    xor ah, ah                                ; 30 e4                       ; 0xc0ae2 vgabios.c:348
    call 02f30h                               ; e8 49 24                    ; 0xc0ae4
    mov ch, al                                ; 88 c5                       ; 0xc0ae7
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0ae9 vgabios.c:349
    je short 00b5ah                           ; 74 6d                       ; 0xc0aeb
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc0aed vgabios.c:353
    lea bx, [bp-010h]                         ; 8d 5e f0                    ; 0xc0af0
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0af3
    call 00a88h                               ; e8 8f ff                    ; 0xc0af6
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc0af9 vgabios.c:354
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0afc
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc0aff vgabios.c:355
    xor al, al                                ; 30 c0                       ; 0xc0b02
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0b04
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0b07
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0b0a vgabios.c:358
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b0d
    call 02f57h                               ; e8 44 24                    ; 0xc0b10
    movzx di, al                              ; 0f b6 f8                    ; 0xc0b13
    inc di                                    ; 47                          ; 0xc0b16
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0b17 vgabios.c:359
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b1a
    call 02f73h                               ; e8 53 24                    ; 0xc0b1d
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc0b20 vgabios.c:361
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0b23
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0b26
    jne short 00b5ah                          ; 75 2d                       ; 0xc0b2b
    mov dx, ax                                ; 89 c2                       ; 0xc0b2d vgabios.c:363
    imul dx, di                               ; 0f af d7                    ; 0xc0b2f
    add dx, dx                                ; 01 d2                       ; 0xc0b32
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0b34
    xor ch, ch                                ; 30 ed                       ; 0xc0b37
    inc dx                                    ; 42                          ; 0xc0b39
    imul cx, dx                               ; 0f af ca                    ; 0xc0b3a
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc0b3d
    imul dx, ax                               ; 0f af d0                    ; 0xc0b41
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc0b44
    add ax, dx                                ; 01 d0                       ; 0xc0b48
    add ax, ax                                ; 01 c0                       ; 0xc0b4a
    mov dx, cx                                ; 89 ca                       ; 0xc0b4c
    add dx, ax                                ; 01 c2                       ; 0xc0b4e
    mov ax, word [bx+04638h]                  ; 8b 87 38 46                 ; 0xc0b50 vgabios.c:364
    call 02f73h                               ; e8 1c 24                    ; 0xc0b54
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc0b57
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0b5a vgabios.c:371
    pop di                                    ; 5f                          ; 0xc0b5d
    pop si                                    ; 5e                          ; 0xc0b5e
    pop cx                                    ; 59                          ; 0xc0b5f
    pop bx                                    ; 5b                          ; 0xc0b60
    pop bp                                    ; 5d                          ; 0xc0b61
    retn                                      ; c3                          ; 0xc0b62
  ; disGetNextSymbol 0xc0b63 LB 0x2f76 -> off=0x10 cb=0000000000000082 uValue=00000000000c0b73 'vga_get_font_info'
    db  08eh, 00bh, 0cdh, 00bh, 0d2h, 00bh, 0dah, 00bh, 0dfh, 00bh, 0e4h, 00bh, 0e9h, 00bh, 0eeh, 00bh
vga_get_font_info:                           ; 0xc0b73 LB 0x82
    push bp                                   ; 55                          ; 0xc0b73 vgabios.c:373
    mov bp, sp                                ; 89 e5                       ; 0xc0b74
    push si                                   ; 56                          ; 0xc0b76
    push di                                   ; 57                          ; 0xc0b77
    push ax                                   ; 50                          ; 0xc0b78
    mov si, dx                                ; 89 d6                       ; 0xc0b79
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc0b7b
    mov bx, cx                                ; 89 cb                       ; 0xc0b7e
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0b80 vgabios.c:378
    jnbe short 00bc4h                         ; 77 3f                       ; 0xc0b83
    mov di, ax                                ; 89 c7                       ; 0xc0b85
    add di, ax                                ; 01 c7                       ; 0xc0b87
    jmp word [cs:di+00b63h]                   ; 2e ff a5 63 0b              ; 0xc0b89
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc0b8e vgabios.c:380
    xor ax, ax                                ; 31 c0                       ; 0xc0b91
    call 02f8fh                               ; e8 f9 23                    ; 0xc0b93
    push SS                                   ; 16                          ; 0xc0b96 vgabios.c:381
    pop ES                                    ; 07                          ; 0xc0b97
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc0b98
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc0b9b
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc0b9e
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0ba1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ba4
    call 02f57h                               ; e8 ad 23                    ; 0xc0ba7
    xor ah, ah                                ; 30 e4                       ; 0xc0baa
    push SS                                   ; 16                          ; 0xc0bac
    pop ES                                    ; 07                          ; 0xc0bad
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0bae
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0bb1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0bb4
    call 02f57h                               ; e8 9d 23                    ; 0xc0bb7
    xor ah, ah                                ; 30 e4                       ; 0xc0bba
    push SS                                   ; 16                          ; 0xc0bbc
    pop ES                                    ; 07                          ; 0xc0bbd
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc0bbe
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0bc1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0bc4
    pop di                                    ; 5f                          ; 0xc0bc7
    pop si                                    ; 5e                          ; 0xc0bc8
    pop bp                                    ; 5d                          ; 0xc0bc9
    retn 00002h                               ; c2 02 00                    ; 0xc0bca
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc0bcd vgabios.c:383
    jmp short 00b91h                          ; eb bf                       ; 0xc0bd0
    mov ax, 05bf2h                            ; b8 f2 5b                    ; 0xc0bd2 vgabios.c:386
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc0bd5
    jmp short 00b96h                          ; eb bc                       ; 0xc0bd8 vgabios.c:387
    mov ax, 053f2h                            ; b8 f2 53                    ; 0xc0bda vgabios.c:389
    jmp short 00bd5h                          ; eb f6                       ; 0xc0bdd
    mov ax, 057f2h                            ; b8 f2 57                    ; 0xc0bdf vgabios.c:392
    jmp short 00bd5h                          ; eb f1                       ; 0xc0be2
    mov ax, 079f2h                            ; b8 f2 79                    ; 0xc0be4 vgabios.c:395
    jmp short 00bd5h                          ; eb ec                       ; 0xc0be7
    mov ax, 069f2h                            ; b8 f2 69                    ; 0xc0be9 vgabios.c:398
    jmp short 00bd5h                          ; eb e7                       ; 0xc0bec
    mov ax, 07b1fh                            ; b8 1f 7b                    ; 0xc0bee vgabios.c:401
    jmp short 00bd5h                          ; eb e2                       ; 0xc0bf1
    jmp short 00bc4h                          ; eb cf                       ; 0xc0bf3 vgabios.c:407
  ; disGetNextSymbol 0xc0bf5 LB 0x2ee4 -> off=0x0 cb=0000000000000139 uValue=00000000000c0bf5 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0bf5 LB 0x139
    push bp                                   ; 55                          ; 0xc0bf5 vgabios.c:420
    mov bp, sp                                ; 89 e5                       ; 0xc0bf6
    push si                                   ; 56                          ; 0xc0bf8
    push di                                   ; 57                          ; 0xc0bf9
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc0bfa
    mov si, dx                                ; 89 d6                       ; 0xc0bfd
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc0bff
    mov di, cx                                ; 89 cf                       ; 0xc0c02
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0c04 vgabios.c:426
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0c07
    call 02f57h                               ; e8 4a 23                    ; 0xc0c0a
    xor ah, ah                                ; 30 e4                       ; 0xc0c0d vgabios.c:427
    call 02f30h                               ; e8 1e 23                    ; 0xc0c0f
    mov cl, al                                ; 88 c1                       ; 0xc0c12
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0c14 vgabios.c:428
    je near 00d27h                            ; 0f 84 0d 01                 ; 0xc0c16
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0c1a vgabios.c:430
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0c1d
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0c20
    je near 00d27h                            ; 0f 84 fe 00                 ; 0xc0c25
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc0c29 vgabios.c:434
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0c2d
    jc short 00c43h                           ; 72 11                       ; 0xc0c30
    jbe short 00c4bh                          ; 76 17                       ; 0xc0c32
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0c34
    je near 00d04h                            ; 0f 84 c9 00                 ; 0xc0c37
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0c3b
    je short 00c4bh                           ; 74 0b                       ; 0xc0c3e
    jmp near 00d22h                           ; e9 df 00                    ; 0xc0c40
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0c43
    je short 00ca3h                           ; 74 5b                       ; 0xc0c46
    jmp near 00d22h                           ; e9 d7 00                    ; 0xc0c48
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0c4b vgabios.c:437
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0c4e
    call 02f73h                               ; e8 1f 23                    ; 0xc0c51
    imul ax, word [bp-00ah]                   ; 0f af 46 f6                 ; 0xc0c54
    mov bx, si                                ; 89 f3                       ; 0xc0c58
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0c5a
    add bx, ax                                ; 01 c3                       ; 0xc0c5d
    mov cx, si                                ; 89 f1                       ; 0xc0c5f vgabios.c:438
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0c61
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0c64
    sar ax, CL                                ; d3 f8                       ; 0xc0c67
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0c69
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0c6c vgabios.c:440
    jmp short 00c79h                          ; eb 08                       ; 0xc0c6f
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0c71
    jnc near 00d24h                           ; 0f 83 ab 00                 ; 0xc0c75
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc0c79 vgabios.c:441
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0c7d
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0c80
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0c82
    out DX, ax                                ; ef                          ; 0xc0c85
    mov dx, bx                                ; 89 da                       ; 0xc0c86 vgabios.c:442
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0c88
    call 02f57h                               ; e8 c9 22                    ; 0xc0c8b
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0c8e
    test al, al                               ; 84 c0                       ; 0xc0c91 vgabios.c:443
    jbe short 00c9eh                          ; 76 09                       ; 0xc0c93
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0c95 vgabios.c:444
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0c98
    sal al, CL                                ; d2 e0                       ; 0xc0c9a
    or ch, al                                 ; 08 c5                       ; 0xc0c9c
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0c9e vgabios.c:445
    jmp short 00c71h                          ; eb ce                       ; 0xc0ca1
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0ca3 vgabios.c:448
    shr ax, 1                                 ; d1 e8                       ; 0xc0ca6
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc0ca8
    mov bx, si                                ; 89 f3                       ; 0xc0cab
    shr bx, 002h                              ; c1 eb 02                    ; 0xc0cad
    add bx, ax                                ; 01 c3                       ; 0xc0cb0
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc0cb2 vgabios.c:449
    je short 00cbbh                           ; 74 03                       ; 0xc0cb6
    add bh, 020h                              ; 80 c7 20                    ; 0xc0cb8 vgabios.c:450
    mov dx, bx                                ; 89 da                       ; 0xc0cbb vgabios.c:451
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc0cbd
    call 02f57h                               ; e8 94 22                    ; 0xc0cc0
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc0cc3 vgabios.c:452
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0cc6
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc0cc9
    jne short 00cebh                          ; 75 1b                       ; 0xc0cce
    mov cx, si                                ; 89 f1                       ; 0xc0cd0 vgabios.c:453
    xor ch, ch                                ; 30 ed                       ; 0xc0cd2
    and cl, 003h                              ; 80 e1 03                    ; 0xc0cd4
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0cd7
    sub bx, cx                                ; 29 cb                       ; 0xc0cda
    mov cx, bx                                ; 89 d9                       ; 0xc0cdc
    add cx, bx                                ; 01 d9                       ; 0xc0cde
    xor ah, ah                                ; 30 e4                       ; 0xc0ce0
    sar ax, CL                                ; d3 f8                       ; 0xc0ce2
    mov ch, al                                ; 88 c5                       ; 0xc0ce4
    and ch, 003h                              ; 80 e5 03                    ; 0xc0ce6
    jmp short 00d24h                          ; eb 39                       ; 0xc0ce9 vgabios.c:454
    mov cx, si                                ; 89 f1                       ; 0xc0ceb vgabios.c:455
    xor ch, ch                                ; 30 ed                       ; 0xc0ced
    and cl, 007h                              ; 80 e1 07                    ; 0xc0cef
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc0cf2
    sub bx, cx                                ; 29 cb                       ; 0xc0cf5
    mov cx, bx                                ; 89 d9                       ; 0xc0cf7
    xor ah, ah                                ; 30 e4                       ; 0xc0cf9
    sar ax, CL                                ; d3 f8                       ; 0xc0cfb
    mov ch, al                                ; 88 c5                       ; 0xc0cfd
    and ch, 001h                              ; 80 e5 01                    ; 0xc0cff
    jmp short 00d24h                          ; eb 20                       ; 0xc0d02 vgabios.c:456
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0d04 vgabios.c:458
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d07
    call 02f73h                               ; e8 66 22                    ; 0xc0d0a
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0d0d
    imul ax, word [bp-00ah]                   ; 0f af 46 f6                 ; 0xc0d10
    mov dx, si                                ; 89 f2                       ; 0xc0d14
    add dx, ax                                ; 01 c2                       ; 0xc0d16
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0d18
    call 02f57h                               ; e8 39 22                    ; 0xc0d1b
    mov ch, al                                ; 88 c5                       ; 0xc0d1e
    jmp short 00d24h                          ; eb 02                       ; 0xc0d20 vgabios.c:460
    xor ch, ch                                ; 30 ed                       ; 0xc0d22 vgabios.c:465
    mov byte [ss:di], ch                      ; 36 88 2d                    ; 0xc0d24 vgabios.c:467
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d27 vgabios.c:468
    pop di                                    ; 5f                          ; 0xc0d2a
    pop si                                    ; 5e                          ; 0xc0d2b
    pop bp                                    ; 5d                          ; 0xc0d2c
    retn                                      ; c3                          ; 0xc0d2d
  ; disGetNextSymbol 0xc0d2e LB 0x2dab -> off=0x0 cb=000000000000008c uValue=00000000000c0d2e 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc0d2e LB 0x8c
    push bp                                   ; 55                          ; 0xc0d2e vgabios.c:473
    mov bp, sp                                ; 89 e5                       ; 0xc0d2f
    push bx                                   ; 53                          ; 0xc0d31
    push cx                                   ; 51                          ; 0xc0d32
    push si                                   ; 56                          ; 0xc0d33
    push di                                   ; 57                          ; 0xc0d34
    push ax                                   ; 50                          ; 0xc0d35
    push ax                                   ; 50                          ; 0xc0d36
    mov bx, ax                                ; 89 c3                       ; 0xc0d37
    mov di, dx                                ; 89 d7                       ; 0xc0d39
    mov dx, 003dah                            ; ba da 03                    ; 0xc0d3b vgabios.c:478
    in AL, DX                                 ; ec                          ; 0xc0d3e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d3f
    xor al, al                                ; 30 c0                       ; 0xc0d41 vgabios.c:479
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0d43
    out DX, AL                                ; ee                          ; 0xc0d46
    xor si, si                                ; 31 f6                       ; 0xc0d47 vgabios.c:481
    cmp si, di                                ; 39 fe                       ; 0xc0d49
    jnc short 00d9fh                          ; 73 52                       ; 0xc0d4b
    mov al, bl                                ; 88 d8                       ; 0xc0d4d vgabios.c:484
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0d4f
    out DX, AL                                ; ee                          ; 0xc0d52
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0d53 vgabios.c:486
    in AL, DX                                 ; ec                          ; 0xc0d56
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d57
    mov cx, ax                                ; 89 c1                       ; 0xc0d59
    in AL, DX                                 ; ec                          ; 0xc0d5b vgabios.c:487
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d5c
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0d5e
    in AL, DX                                 ; ec                          ; 0xc0d61 vgabios.c:488
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d62
    xor ch, ch                                ; 30 ed                       ; 0xc0d64 vgabios.c:491
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc0d66
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc0d69
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4                 ; 0xc0d6c
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc0d70
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc0d74
    xor ah, ah                                ; 30 e4                       ; 0xc0d77
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc0d79
    add cx, ax                                ; 01 c1                       ; 0xc0d7c
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc0d7e
    sar cx, 008h                              ; c1 f9 08                    ; 0xc0d82
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc0d85 vgabios.c:493
    jbe short 00d8dh                          ; 76 03                       ; 0xc0d88
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc0d8a
    mov al, bl                                ; 88 d8                       ; 0xc0d8d vgabios.c:496
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0d8f
    out DX, AL                                ; ee                          ; 0xc0d92
    mov al, cl                                ; 88 c8                       ; 0xc0d93 vgabios.c:498
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0d95
    out DX, AL                                ; ee                          ; 0xc0d98
    out DX, AL                                ; ee                          ; 0xc0d99 vgabios.c:499
    out DX, AL                                ; ee                          ; 0xc0d9a vgabios.c:500
    inc bx                                    ; 43                          ; 0xc0d9b vgabios.c:501
    inc si                                    ; 46                          ; 0xc0d9c vgabios.c:502
    jmp short 00d49h                          ; eb aa                       ; 0xc0d9d
    mov dx, 003dah                            ; ba da 03                    ; 0xc0d9f vgabios.c:503
    in AL, DX                                 ; ec                          ; 0xc0da2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0da3
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0da5 vgabios.c:504
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0da7
    out DX, AL                                ; ee                          ; 0xc0daa
    mov dx, 003dah                            ; ba da 03                    ; 0xc0dab vgabios.c:506
    in AL, DX                                 ; ec                          ; 0xc0dae
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0daf
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0db1 vgabios.c:508
    pop di                                    ; 5f                          ; 0xc0db4
    pop si                                    ; 5e                          ; 0xc0db5
    pop cx                                    ; 59                          ; 0xc0db6
    pop bx                                    ; 5b                          ; 0xc0db7
    pop bp                                    ; 5d                          ; 0xc0db8
    retn                                      ; c3                          ; 0xc0db9
  ; disGetNextSymbol 0xc0dba LB 0x2d1f -> off=0x0 cb=00000000000000a4 uValue=00000000000c0dba 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc0dba LB 0xa4
    push bp                                   ; 55                          ; 0xc0dba vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0dbb
    push bx                                   ; 53                          ; 0xc0dbd
    push cx                                   ; 51                          ; 0xc0dbe
    push si                                   ; 56                          ; 0xc0dbf
    push di                                   ; 57                          ; 0xc0dc0
    mov ch, al                                ; 88 c5                       ; 0xc0dc1
    mov cl, dl                                ; 88 d1                       ; 0xc0dc3
    and ch, 03fh                              ; 80 e5 3f                    ; 0xc0dc5 vgabios.c:515
    and cl, 01fh                              ; 80 e1 1f                    ; 0xc0dc8 vgabios.c:516
    movzx di, ch                              ; 0f b6 fd                    ; 0xc0dcb vgabios.c:518
    mov bx, di                                ; 89 fb                       ; 0xc0dce
    sal bx, 008h                              ; c1 e3 08                    ; 0xc0dd0
    movzx si, cl                              ; 0f b6 f1                    ; 0xc0dd3
    add bx, si                                ; 01 f3                       ; 0xc0dd6
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0dd8 vgabios.c:519
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ddb
    call 02f81h                               ; e8 a0 21                    ; 0xc0dde
    mov dx, 00089h                            ; ba 89 00                    ; 0xc0de1 vgabios.c:521
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0de4
    call 02f57h                               ; e8 6d 21                    ; 0xc0de7
    mov bl, al                                ; 88 c3                       ; 0xc0dea
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0dec vgabios.c:522
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0def
    call 02f73h                               ; e8 7e 21                    ; 0xc0df2
    mov dx, ax                                ; 89 c2                       ; 0xc0df5
    test bl, 001h                             ; f6 c3 01                    ; 0xc0df7 vgabios.c:523
    je short 00e33h                           ; 74 37                       ; 0xc0dfa
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0dfc
    jbe short 00e33h                          ; 76 32                       ; 0xc0dff
    cmp cl, 008h                              ; 80 f9 08                    ; 0xc0e01
    jnc short 00e33h                          ; 73 2d                       ; 0xc0e04
    cmp ch, 020h                              ; 80 fd 20                    ; 0xc0e06
    jnc short 00e33h                          ; 73 28                       ; 0xc0e09
    inc di                                    ; 47                          ; 0xc0e0b vgabios.c:525
    cmp si, di                                ; 39 fe                       ; 0xc0e0c
    je short 00e19h                           ; 74 09                       ; 0xc0e0e
    imul ax, di                               ; 0f af c7                    ; 0xc0e10 vgabios.c:527
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e13
    dec ax                                    ; 48                          ; 0xc0e16
    jmp short 00e24h                          ; eb 0b                       ; 0xc0e17 vgabios.c:529
    lea si, [di+001h]                         ; 8d 75 01                    ; 0xc0e19 vgabios.c:531
    imul ax, si                               ; 0f af c6                    ; 0xc0e1c
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e1f
    dec ax                                    ; 48                          ; 0xc0e22
    dec ax                                    ; 48                          ; 0xc0e23
    mov ch, al                                ; 88 c5                       ; 0xc0e24
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc0e26 vgabios.c:533
    inc ax                                    ; 40                          ; 0xc0e29
    imul ax, dx                               ; 0f af c2                    ; 0xc0e2a
    shr ax, 003h                              ; c1 e8 03                    ; 0xc0e2d
    dec ax                                    ; 48                          ; 0xc0e30
    mov cl, al                                ; 88 c1                       ; 0xc0e31
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0e33 vgabios.c:537
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e36
    call 02f73h                               ; e8 37 21                    ; 0xc0e39
    mov bx, ax                                ; 89 c3                       ; 0xc0e3c
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc0e3e vgabios.c:538
    mov dx, bx                                ; 89 da                       ; 0xc0e40
    out DX, AL                                ; ee                          ; 0xc0e42
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc0e43 vgabios.c:539
    mov al, ch                                ; 88 e8                       ; 0xc0e46
    mov dx, si                                ; 89 f2                       ; 0xc0e48
    out DX, AL                                ; ee                          ; 0xc0e4a
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc0e4b vgabios.c:540
    mov dx, bx                                ; 89 da                       ; 0xc0e4d
    out DX, AL                                ; ee                          ; 0xc0e4f
    mov al, cl                                ; 88 c8                       ; 0xc0e50 vgabios.c:541
    mov dx, si                                ; 89 f2                       ; 0xc0e52
    out DX, AL                                ; ee                          ; 0xc0e54
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e55 vgabios.c:542
    pop di                                    ; 5f                          ; 0xc0e58
    pop si                                    ; 5e                          ; 0xc0e59
    pop cx                                    ; 59                          ; 0xc0e5a
    pop bx                                    ; 5b                          ; 0xc0e5b
    pop bp                                    ; 5d                          ; 0xc0e5c
    retn                                      ; c3                          ; 0xc0e5d
  ; disGetNextSymbol 0xc0e5e LB 0x2c7b -> off=0x0 cb=00000000000000a2 uValue=00000000000c0e5e 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc0e5e LB 0xa2
    push bp                                   ; 55                          ; 0xc0e5e vgabios.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc0e5f
    push bx                                   ; 53                          ; 0xc0e61
    push cx                                   ; 51                          ; 0xc0e62
    push si                                   ; 56                          ; 0xc0e63
    push ax                                   ; 50                          ; 0xc0e64
    push ax                                   ; 50                          ; 0xc0e65
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0e66
    mov cx, dx                                ; 89 d1                       ; 0xc0e69
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0e6b vgabios.c:551
    jnbe near 00ef8h                          ; 0f 87 87 00                 ; 0xc0e6d
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e71 vgabios.c:554
    add dx, dx                                ; 01 d2                       ; 0xc0e74
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0e76
    mov bx, cx                                ; 89 cb                       ; 0xc0e79
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e7b
    call 02f81h                               ; e8 00 21                    ; 0xc0e7e
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc0e81 vgabios.c:557
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e84
    call 02f57h                               ; e8 cd 20                    ; 0xc0e87
    cmp al, byte [bp-008h]                    ; 3a 46 f8                    ; 0xc0e8a vgabios.c:558
    jne short 00ef8h                          ; 75 69                       ; 0xc0e8d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0e8f vgabios.c:561
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e92
    call 02f73h                               ; e8 db 20                    ; 0xc0e95
    mov bx, ax                                ; 89 c3                       ; 0xc0e98
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0e9a vgabios.c:562
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e9d
    call 02f57h                               ; e8 b4 20                    ; 0xc0ea0
    xor ah, ah                                ; 30 e4                       ; 0xc0ea3
    mov dx, ax                                ; 89 c2                       ; 0xc0ea5
    inc dx                                    ; 42                          ; 0xc0ea7
    mov al, cl                                ; 88 c8                       ; 0xc0ea8 vgabios.c:564
    xor cl, cl                                ; 30 c9                       ; 0xc0eaa
    shr cx, 008h                              ; c1 e9 08                    ; 0xc0eac
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc0eaf
    imul dx, bx                               ; 0f af d3                    ; 0xc0eb2 vgabios.c:567
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0eb5
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8                 ; 0xc0eb8
    inc dx                                    ; 42                          ; 0xc0ebc
    imul dx, cx                               ; 0f af d1                    ; 0xc0ebd
    mov si, ax                                ; 89 c6                       ; 0xc0ec0
    add si, dx                                ; 01 d6                       ; 0xc0ec2
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc0ec4
    imul bx, dx                               ; 0f af da                    ; 0xc0ec8
    add si, bx                                ; 01 de                       ; 0xc0ecb
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0ecd vgabios.c:570
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ed0
    call 02f73h                               ; e8 9d 20                    ; 0xc0ed3
    mov bx, ax                                ; 89 c3                       ; 0xc0ed6
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc0ed8 vgabios.c:571
    mov dx, bx                                ; 89 da                       ; 0xc0eda
    out DX, AL                                ; ee                          ; 0xc0edc
    mov ax, si                                ; 89 f0                       ; 0xc0edd vgabios.c:572
    xor al, al                                ; 30 c0                       ; 0xc0edf
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0ee1
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc0ee4
    mov dx, cx                                ; 89 ca                       ; 0xc0ee7
    out DX, AL                                ; ee                          ; 0xc0ee9
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc0eea vgabios.c:573
    mov dx, bx                                ; 89 da                       ; 0xc0eec
    out DX, AL                                ; ee                          ; 0xc0eee
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc0eef vgabios.c:574
    mov ax, si                                ; 89 f0                       ; 0xc0ef3
    mov dx, cx                                ; 89 ca                       ; 0xc0ef5
    out DX, AL                                ; ee                          ; 0xc0ef7
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0ef8 vgabios.c:576
    pop si                                    ; 5e                          ; 0xc0efb
    pop cx                                    ; 59                          ; 0xc0efc
    pop bx                                    ; 5b                          ; 0xc0efd
    pop bp                                    ; 5d                          ; 0xc0efe
    retn                                      ; c3                          ; 0xc0eff
  ; disGetNextSymbol 0xc0f00 LB 0x2bd9 -> off=0x0 cb=00000000000000dc uValue=00000000000c0f00 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc0f00 LB 0xdc
    push bp                                   ; 55                          ; 0xc0f00 vgabios.c:579
    mov bp, sp                                ; 89 e5                       ; 0xc0f01
    push bx                                   ; 53                          ; 0xc0f03
    push cx                                   ; 51                          ; 0xc0f04
    push dx                                   ; 52                          ; 0xc0f05
    push si                                   ; 56                          ; 0xc0f06
    push di                                   ; 57                          ; 0xc0f07
    push ax                                   ; 50                          ; 0xc0f08
    push ax                                   ; 50                          ; 0xc0f09
    mov cl, al                                ; 88 c1                       ; 0xc0f0a
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0f0c vgabios.c:585
    jnbe near 00fd2h                          ; 0f 87 c0 00                 ; 0xc0f0e
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0f12 vgabios.c:588
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f15
    call 02f57h                               ; e8 3c 20                    ; 0xc0f18
    xor ah, ah                                ; 30 e4                       ; 0xc0f1b vgabios.c:589
    call 02f30h                               ; e8 10 20                    ; 0xc0f1d
    mov ch, al                                ; 88 c5                       ; 0xc0f20
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f22 vgabios.c:590
    je near 00fd2h                            ; 0f 84 aa 00                 ; 0xc0f24
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc0f28 vgabios.c:593
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0f2b
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc0f2e
    call 00a88h                               ; e8 54 fb                    ; 0xc0f31
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc0f34 vgabios.c:595
    mov si, bx                                ; 89 de                       ; 0xc0f37
    sal si, 003h                              ; c1 e6 03                    ; 0xc0f39
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc0f3c
    jne short 00f83h                          ; 75 40                       ; 0xc0f41
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0f43 vgabios.c:598
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f46
    call 02f73h                               ; e8 27 20                    ; 0xc0f49
    mov bx, ax                                ; 89 c3                       ; 0xc0f4c
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0f4e vgabios.c:599
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f51
    call 02f57h                               ; e8 00 20                    ; 0xc0f54
    xor ah, ah                                ; 30 e4                       ; 0xc0f57
    inc ax                                    ; 40                          ; 0xc0f59
    mov si, bx                                ; 89 de                       ; 0xc0f5a vgabios.c:602
    imul si, ax                               ; 0f af f0                    ; 0xc0f5c
    mov ax, si                                ; 89 f0                       ; 0xc0f5f
    add ax, si                                ; 01 f0                       ; 0xc0f61
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0f63
    movzx di, cl                              ; 0f b6 f9                    ; 0xc0f65
    mov bx, ax                                ; 89 c3                       ; 0xc0f68
    inc bx                                    ; 43                          ; 0xc0f6a
    imul bx, di                               ; 0f af df                    ; 0xc0f6b
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc0f6e vgabios.c:603
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f71
    call 02f81h                               ; e8 0a 20                    ; 0xc0f74
    or si, 000ffh                             ; 81 ce ff 00                 ; 0xc0f77 vgabios.c:606
    lea bx, [si+001h]                         ; 8d 5c 01                    ; 0xc0f7b
    imul bx, di                               ; 0f af df                    ; 0xc0f7e
    jmp short 00f95h                          ; eb 12                       ; 0xc0f81 vgabios.c:608
    movzx bx, byte [bx+046b4h]                ; 0f b6 9f b4 46              ; 0xc0f83 vgabios.c:610
    sal bx, 006h                              ; c1 e3 06                    ; 0xc0f88
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc0f8b
    mov bx, word [bx+046cbh]                  ; 8b 9f cb 46                 ; 0xc0f8e
    imul bx, ax                               ; 0f af d8                    ; 0xc0f92
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0f95 vgabios.c:614
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f98
    call 02f73h                               ; e8 d5 1f                    ; 0xc0f9b
    mov si, ax                                ; 89 c6                       ; 0xc0f9e
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc0fa0 vgabios.c:615
    mov dx, si                                ; 89 f2                       ; 0xc0fa2
    out DX, AL                                ; ee                          ; 0xc0fa4
    mov ax, bx                                ; 89 d8                       ; 0xc0fa5 vgabios.c:616
    xor al, bl                                ; 30 d8                       ; 0xc0fa7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0fa9
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc0fac
    mov dx, di                                ; 89 fa                       ; 0xc0faf
    out DX, AL                                ; ee                          ; 0xc0fb1
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc0fb2 vgabios.c:617
    mov dx, si                                ; 89 f2                       ; 0xc0fb4
    out DX, AL                                ; ee                          ; 0xc0fb6
    mov al, bl                                ; 88 d8                       ; 0xc0fb7 vgabios.c:618
    mov dx, di                                ; 89 fa                       ; 0xc0fb9
    out DX, AL                                ; ee                          ; 0xc0fbb
    movzx si, cl                              ; 0f b6 f1                    ; 0xc0fbc vgabios.c:621
    mov bx, si                                ; 89 f3                       ; 0xc0fbf
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc0fc1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fc4
    call 02f65h                               ; e8 9b 1f                    ; 0xc0fc7
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc0fca vgabios.c:628
    mov ax, si                                ; 89 f0                       ; 0xc0fcd
    call 00e5eh                               ; e8 8c fe                    ; 0xc0fcf
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc0fd2 vgabios.c:629
    pop di                                    ; 5f                          ; 0xc0fd5
    pop si                                    ; 5e                          ; 0xc0fd6
    pop dx                                    ; 5a                          ; 0xc0fd7
    pop cx                                    ; 59                          ; 0xc0fd8
    pop bx                                    ; 5b                          ; 0xc0fd9
    pop bp                                    ; 5d                          ; 0xc0fda
    retn                                      ; c3                          ; 0xc0fdb
  ; disGetNextSymbol 0xc0fdc LB 0x2afd -> off=0x0 cb=00000000000003aa uValue=00000000000c0fdc 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc0fdc LB 0x3aa
    push bp                                   ; 55                          ; 0xc0fdc vgabios.c:649
    mov bp, sp                                ; 89 e5                       ; 0xc0fdd
    push bx                                   ; 53                          ; 0xc0fdf
    push cx                                   ; 51                          ; 0xc0fe0
    push dx                                   ; 52                          ; 0xc0fe1
    push si                                   ; 56                          ; 0xc0fe2
    push di                                   ; 57                          ; 0xc0fe3
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0fe4
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0fe7
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0fea vgabios.c:653
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0fec
    call 007bfh                               ; e8 cd f7                    ; 0xc0fef vgabios.c:660
    test ax, ax                               ; 85 c0                       ; 0xc0ff2
    je short 01002h                           ; 74 0c                       ; 0xc0ff4
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc0ff6 vgabios.c:662
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0ff8
    out DX, AL                                ; ee                          ; 0xc0ffb
    xor al, al                                ; 30 c0                       ; 0xc0ffc vgabios.c:663
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0ffe
    out DX, AL                                ; ee                          ; 0xc1001
    and byte [bp-010h], 07fh                  ; 80 66 f0 7f                 ; 0xc1002 vgabios.c:668
    cmp byte [bp-010h], 007h                  ; 80 7e f0 07                 ; 0xc1006 vgabios.c:672
    jne short 01010h                          ; 75 04                       ; 0xc100a
    mov byte [bp-010h], 000h                  ; c6 46 f0 00                 ; 0xc100c vgabios.c:673
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1010 vgabios.c:676
    call 02f30h                               ; e8 19 1f                    ; 0xc1014
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1017
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc101a vgabios.c:682
    je near 0137ch                            ; 0f 84 5c 03                 ; 0xc101c
    movzx si, al                              ; 0f b6 f0                    ; 0xc1020 vgabios.c:685
    mov al, byte [si+046b4h]                  ; 8a 84 b4 46                 ; 0xc1023
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1027
    movzx bx, al                              ; 0f b6 d8                    ; 0xc102a vgabios.c:686
    sal bx, 006h                              ; c1 e3 06                    ; 0xc102d
    movzx ax, byte [bx+046c8h]                ; 0f b6 87 c8 46              ; 0xc1030
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1035
    movzx ax, byte [bx+046c9h]                ; 0f b6 87 c9 46              ; 0xc1038 vgabios.c:687
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc103d
    movzx ax, byte [bx+046cah]                ; 0f b6 87 ca 46              ; 0xc1040 vgabios.c:688
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1045
    mov dx, 00087h                            ; ba 87 00                    ; 0xc1048 vgabios.c:691
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc104b
    call 02f57h                               ; e8 06 1f                    ; 0xc104e
    mov dx, 00088h                            ; ba 88 00                    ; 0xc1051 vgabios.c:694
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1054
    call 02f57h                               ; e8 fd 1e                    ; 0xc1057
    mov dx, 00089h                            ; ba 89 00                    ; 0xc105a vgabios.c:697
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc105d
    call 02f57h                               ; e8 f4 1e                    ; 0xc1060
    mov cl, al                                ; 88 c1                       ; 0xc1063
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1065 vgabios.c:703
    jne near 010f5h                           ; 0f 85 8a 00                 ; 0xc1067
    mov bx, si                                ; 89 f3                       ; 0xc106b vgabios.c:705
    sal bx, 003h                              ; c1 e3 03                    ; 0xc106d
    mov al, byte [bx+0463ah]                  ; 8a 87 3a 46                 ; 0xc1070
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1074
    out DX, AL                                ; ee                          ; 0xc1077
    xor al, al                                ; 30 c0                       ; 0xc1078 vgabios.c:708
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc107a
    out DX, AL                                ; ee                          ; 0xc107d
    mov bl, byte [bx+0463bh]                  ; 8a 9f 3b 46                 ; 0xc107e vgabios.c:711
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc1082
    jc short 01095h                           ; 72 0e                       ; 0xc1085
    jbe short 0109eh                          ; 76 15                       ; 0xc1087
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1089
    je short 010a8h                           ; 74 1a                       ; 0xc108c
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc108e
    je short 010a3h                           ; 74 10                       ; 0xc1091
    jmp short 010abh                          ; eb 16                       ; 0xc1093
    test bl, bl                               ; 84 db                       ; 0xc1095
    jne short 010abh                          ; 75 12                       ; 0xc1097
    mov di, 04e48h                            ; bf 48 4e                    ; 0xc1099 vgabios.c:713
    jmp short 010abh                          ; eb 0d                       ; 0xc109c vgabios.c:714
    mov di, 04f08h                            ; bf 08 4f                    ; 0xc109e vgabios.c:716
    jmp short 010abh                          ; eb 08                       ; 0xc10a1 vgabios.c:717
    mov di, 04fc8h                            ; bf c8 4f                    ; 0xc10a3 vgabios.c:719
    jmp short 010abh                          ; eb 03                       ; 0xc10a6 vgabios.c:720
    mov di, 05088h                            ; bf 88 50                    ; 0xc10a8 vgabios.c:722
    xor bx, bx                                ; 31 db                       ; 0xc10ab vgabios.c:726
    jmp short 010beh                          ; eb 0f                       ; 0xc10ad
    xor al, al                                ; 30 c0                       ; 0xc10af vgabios.c:733
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10b1
    out DX, AL                                ; ee                          ; 0xc10b4
    out DX, AL                                ; ee                          ; 0xc10b5 vgabios.c:734
    out DX, AL                                ; ee                          ; 0xc10b6 vgabios.c:735
    inc bx                                    ; 43                          ; 0xc10b7 vgabios.c:737
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc10b8
    jnc short 010e8h                          ; 73 2a                       ; 0xc10bc
    movzx si, byte [bp-012h]                  ; 0f b6 76 ee                 ; 0xc10be
    sal si, 003h                              ; c1 e6 03                    ; 0xc10c2
    movzx si, byte [si+0463bh]                ; 0f b6 b4 3b 46              ; 0xc10c5
    movzx ax, byte [si+046c4h]                ; 0f b6 84 c4 46              ; 0xc10ca
    cmp bx, ax                                ; 39 c3                       ; 0xc10cf
    jnbe short 010afh                         ; 77 dc                       ; 0xc10d1
    imul si, bx, strict byte 00003h           ; 6b f3 03                    ; 0xc10d3
    add si, di                                ; 01 fe                       ; 0xc10d6
    mov al, byte [si]                         ; 8a 04                       ; 0xc10d8
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10da
    out DX, AL                                ; ee                          ; 0xc10dd
    mov al, byte [si+001h]                    ; 8a 44 01                    ; 0xc10de
    out DX, AL                                ; ee                          ; 0xc10e1
    mov al, byte [si+002h]                    ; 8a 44 02                    ; 0xc10e2
    out DX, AL                                ; ee                          ; 0xc10e5
    jmp short 010b7h                          ; eb cf                       ; 0xc10e6
    test cl, 002h                             ; f6 c1 02                    ; 0xc10e8 vgabios.c:738
    je short 010f5h                           ; 74 08                       ; 0xc10eb
    mov dx, 00100h                            ; ba 00 01                    ; 0xc10ed vgabios.c:740
    xor ax, ax                                ; 31 c0                       ; 0xc10f0
    call 00d2eh                               ; e8 39 fc                    ; 0xc10f2
    mov dx, 003dah                            ; ba da 03                    ; 0xc10f5 vgabios.c:745
    in AL, DX                                 ; ec                          ; 0xc10f8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10f9
    xor bx, bx                                ; 31 db                       ; 0xc10fb vgabios.c:748
    jmp short 01104h                          ; eb 05                       ; 0xc10fd
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc10ff
    jnbe short 0111bh                         ; 77 17                       ; 0xc1102
    mov al, bl                                ; 88 d8                       ; 0xc1104 vgabios.c:749
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1106
    out DX, AL                                ; ee                          ; 0xc1109
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc110a vgabios.c:750
    sal si, 006h                              ; c1 e6 06                    ; 0xc110e
    add si, bx                                ; 01 de                       ; 0xc1111
    mov al, byte [si+046ebh]                  ; 8a 84 eb 46                 ; 0xc1113
    out DX, AL                                ; ee                          ; 0xc1117
    inc bx                                    ; 43                          ; 0xc1118 vgabios.c:751
    jmp short 010ffh                          ; eb e4                       ; 0xc1119
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc111b vgabios.c:752
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc111d
    out DX, AL                                ; ee                          ; 0xc1120
    xor al, al                                ; 30 c0                       ; 0xc1121 vgabios.c:753
    out DX, AL                                ; ee                          ; 0xc1123
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1124 vgabios.c:756
    out DX, AL                                ; ee                          ; 0xc1127
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1128 vgabios.c:757
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc112a
    out DX, AL                                ; ee                          ; 0xc112d
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc112e vgabios.c:758
    jmp short 01138h                          ; eb 05                       ; 0xc1131
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc1133
    jnbe short 01152h                         ; 77 1a                       ; 0xc1136
    mov al, bl                                ; 88 d8                       ; 0xc1138 vgabios.c:759
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc113a
    out DX, AL                                ; ee                          ; 0xc113d
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc113e vgabios.c:760
    sal si, 006h                              ; c1 e6 06                    ; 0xc1142
    add si, bx                                ; 01 de                       ; 0xc1145
    mov al, byte [si+046cch]                  ; 8a 84 cc 46                 ; 0xc1147
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc114b
    out DX, AL                                ; ee                          ; 0xc114e
    inc bx                                    ; 43                          ; 0xc114f vgabios.c:761
    jmp short 01133h                          ; eb e1                       ; 0xc1150
    xor bx, bx                                ; 31 db                       ; 0xc1152 vgabios.c:764
    jmp short 0115bh                          ; eb 05                       ; 0xc1154
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1156
    jnbe short 01175h                         ; 77 1a                       ; 0xc1159
    mov al, bl                                ; 88 d8                       ; 0xc115b vgabios.c:765
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc115d
    out DX, AL                                ; ee                          ; 0xc1160
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc1161 vgabios.c:766
    sal si, 006h                              ; c1 e6 06                    ; 0xc1165
    add si, bx                                ; 01 de                       ; 0xc1168
    mov al, byte [si+046ffh]                  ; 8a 84 ff 46                 ; 0xc116a
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc116e
    out DX, AL                                ; ee                          ; 0xc1171
    inc bx                                    ; 43                          ; 0xc1172 vgabios.c:767
    jmp short 01156h                          ; eb e1                       ; 0xc1173
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1175 vgabios.c:770
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1179
    cmp byte [bx+04636h], 001h                ; 80 bf 36 46 01              ; 0xc117c
    jne short 01188h                          ; 75 05                       ; 0xc1181
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1183
    jmp short 0118bh                          ; eb 03                       ; 0xc1186
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc1188
    mov word [bp-01ah], dx                    ; 89 56 e6                    ; 0xc118b
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc118e vgabios.c:773
    out DX, ax                                ; ef                          ; 0xc1191
    xor bx, bx                                ; 31 db                       ; 0xc1192 vgabios.c:775
    jmp short 0119bh                          ; eb 05                       ; 0xc1194
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc1196
    jnbe short 011b5h                         ; 77 1a                       ; 0xc1199
    mov al, bl                                ; 88 d8                       ; 0xc119b vgabios.c:776
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc119d
    out DX, AL                                ; ee                          ; 0xc11a0
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc11a1 vgabios.c:777
    sal si, 006h                              ; c1 e6 06                    ; 0xc11a5
    mov di, si                                ; 89 f7                       ; 0xc11a8
    add di, bx                                ; 01 df                       ; 0xc11aa
    inc dx                                    ; 42                          ; 0xc11ac
    mov al, byte [di+046d2h]                  ; 8a 85 d2 46                 ; 0xc11ad
    out DX, AL                                ; ee                          ; 0xc11b1
    inc bx                                    ; 43                          ; 0xc11b2 vgabios.c:778
    jmp short 01196h                          ; eb e1                       ; 0xc11b3
    mov al, byte [si+046d1h]                  ; 8a 84 d1 46                 ; 0xc11b5 vgabios.c:781
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc11b9
    out DX, AL                                ; ee                          ; 0xc11bc
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc11bd vgabios.c:784
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc11bf
    out DX, AL                                ; ee                          ; 0xc11c2
    mov dx, 003dah                            ; ba da 03                    ; 0xc11c3 vgabios.c:785
    in AL, DX                                 ; ec                          ; 0xc11c6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc11c7
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc11c9 vgabios.c:787
    jne short 0122eh                          ; 75 5f                       ; 0xc11cd
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc11cf vgabios.c:789
    sal bx, 003h                              ; c1 e3 03                    ; 0xc11d3
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc11d6
    jne short 011f0h                          ; 75 13                       ; 0xc11db
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc11dd vgabios.c:791
    mov cx, 04000h                            ; b9 00 40                    ; 0xc11e1
    mov ax, 00720h                            ; b8 20 07                    ; 0xc11e4
    xor di, di                                ; 31 ff                       ; 0xc11e7
    cld                                       ; fc                          ; 0xc11e9
    jcxz 011eeh                               ; e3 02                       ; 0xc11ea
    rep stosw                                 ; f3 ab                       ; 0xc11ec
    jmp short 0122eh                          ; eb 3e                       ; 0xc11ee vgabios.c:793
    cmp byte [bp-010h], 00dh                  ; 80 7e f0 0d                 ; 0xc11f0 vgabios.c:795
    jnc short 01208h                          ; 73 12                       ; 0xc11f4
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc11f6 vgabios.c:797
    mov cx, 04000h                            ; b9 00 40                    ; 0xc11fa
    xor ax, ax                                ; 31 c0                       ; 0xc11fd
    xor di, di                                ; 31 ff                       ; 0xc11ff
    cld                                       ; fc                          ; 0xc1201
    jcxz 01206h                               ; e3 02                       ; 0xc1202
    rep stosw                                 ; f3 ab                       ; 0xc1204
    jmp short 0122eh                          ; eb 26                       ; 0xc1206 vgabios.c:799
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc1208 vgabios.c:801
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc120a
    out DX, AL                                ; ee                          ; 0xc120d
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc120e vgabios.c:802
    in AL, DX                                 ; ec                          ; 0xc1211
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1212
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1214
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1217 vgabios.c:803
    out DX, AL                                ; ee                          ; 0xc1219
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc121a vgabios.c:804
    mov cx, 08000h                            ; b9 00 80                    ; 0xc121e
    xor ax, ax                                ; 31 c0                       ; 0xc1221
    xor di, di                                ; 31 ff                       ; 0xc1223
    cld                                       ; fc                          ; 0xc1225
    jcxz 0122ah                               ; e3 02                       ; 0xc1226
    rep stosw                                 ; f3 ab                       ; 0xc1228
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc122a vgabios.c:805
    out DX, AL                                ; ee                          ; 0xc122d
    movzx si, byte [bp-010h]                  ; 0f b6 76 f0                 ; 0xc122e vgabios.c:811
    mov bx, si                                ; 89 f3                       ; 0xc1232
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1234
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1237
    call 02f65h                               ; e8 28 1d                    ; 0xc123a
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc123d vgabios.c:812
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc1240
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1243
    call 02f81h                               ; e8 38 1d                    ; 0xc1246
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1249 vgabios.c:813
    sal bx, 006h                              ; c1 e3 06                    ; 0xc124d
    mov bx, word [bx+046cbh]                  ; 8b 9f cb 46                 ; 0xc1250
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc1254
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1257
    call 02f81h                               ; e8 24 1d                    ; 0xc125a
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc125d vgabios.c:814
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc1260
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1263
    call 02f81h                               ; e8 18 1d                    ; 0xc1266
    movzx bx, byte [bp-018h]                  ; 0f b6 5e e8                 ; 0xc1269 vgabios.c:815
    mov dx, 00084h                            ; ba 84 00                    ; 0xc126d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1270
    call 02f65h                               ; e8 ef 1c                    ; 0xc1273
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc1276 vgabios.c:816
    mov dx, 00085h                            ; ba 85 00                    ; 0xc1279
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc127c
    call 02f81h                               ; e8 ff 1c                    ; 0xc127f
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1282 vgabios.c:817
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1285
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1287
    mov dx, 00087h                            ; ba 87 00                    ; 0xc128a
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc128d
    call 02f65h                               ; e8 d2 1c                    ; 0xc1290
    mov bx, 000f9h                            ; bb f9 00                    ; 0xc1293 vgabios.c:818
    mov dx, 00088h                            ; ba 88 00                    ; 0xc1296
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1299
    call 02f65h                               ; e8 c6 1c                    ; 0xc129c
    mov dx, 00089h                            ; ba 89 00                    ; 0xc129f vgabios.c:819
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12a2
    call 02f57h                               ; e8 af 1c                    ; 0xc12a5
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc12a8
    movzx bx, al                              ; 0f b6 d8                    ; 0xc12aa
    mov dx, 00089h                            ; ba 89 00                    ; 0xc12ad
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12b0
    call 02f65h                               ; e8 af 1c                    ; 0xc12b3
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc12b6 vgabios.c:822
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc12b9
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12bc
    call 02f65h                               ; e8 a3 1c                    ; 0xc12bf
    mov cx, ds                                ; 8c d9                       ; 0xc12c2 vgabios.c:823
    mov bx, 053d6h                            ; bb d6 53                    ; 0xc12c4
    mov dx, 000a8h                            ; ba a8 00                    ; 0xc12c7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ca
    call 02fa1h                               ; e8 d1 1c                    ; 0xc12cd
    cmp byte [bp-010h], 007h                  ; 80 7e f0 07                 ; 0xc12d0 vgabios.c:825
    jnbe short 012feh                         ; 77 28                       ; 0xc12d4
    movzx bx, byte [si+07c63h]                ; 0f b6 9c 63 7c              ; 0xc12d6 vgabios.c:827
    mov dx, strict word 00065h                ; ba 65 00                    ; 0xc12db
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12de
    call 02f65h                               ; e8 81 1c                    ; 0xc12e1
    cmp byte [bp-010h], 006h                  ; 80 7e f0 06                 ; 0xc12e4 vgabios.c:828
    jne short 012efh                          ; 75 05                       ; 0xc12e8
    mov dx, strict word 0003fh                ; ba 3f 00                    ; 0xc12ea
    jmp short 012f2h                          ; eb 03                       ; 0xc12ed
    mov dx, strict word 00030h                ; ba 30 00                    ; 0xc12ef
    movzx bx, dl                              ; 0f b6 da                    ; 0xc12f2
    mov dx, strict word 00066h                ; ba 66 00                    ; 0xc12f5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12f8
    call 02f65h                               ; e8 67 1c                    ; 0xc12fb
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc12fe vgabios.c:832
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1302
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1305
    jne short 01315h                          ; 75 09                       ; 0xc130a
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc130c vgabios.c:834
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc130f
    call 00dbah                               ; e8 a5 fa                    ; 0xc1312
    xor bx, bx                                ; 31 db                       ; 0xc1315 vgabios.c:838
    jmp short 0131eh                          ; eb 05                       ; 0xc1317
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1319
    jnc short 01329h                          ; 73 0b                       ; 0xc131c
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc131e vgabios.c:839
    xor dx, dx                                ; 31 d2                       ; 0xc1321
    call 00e5eh                               ; e8 38 fb                    ; 0xc1323
    inc bx                                    ; 43                          ; 0xc1326
    jmp short 01319h                          ; eb f0                       ; 0xc1327
    xor ax, ax                                ; 31 c0                       ; 0xc1329 vgabios.c:842
    call 00f00h                               ; e8 d2 fb                    ; 0xc132b
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc132e vgabios.c:845
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1332
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1335
    jne short 0134ch                          ; 75 10                       ; 0xc133a
    xor bl, bl                                ; 30 db                       ; 0xc133c vgabios.c:847
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc133e
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1340
    int 010h                                  ; cd 10                       ; 0xc1342
    xor bl, bl                                ; 30 db                       ; 0xc1344 vgabios.c:848
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1346
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1348
    int 010h                                  ; cd 10                       ; 0xc134a
    mov dx, 057f2h                            ; ba f2 57                    ; 0xc134c vgabios.c:852
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc134f
    call 00a00h                               ; e8 ab f6                    ; 0xc1352
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1355 vgabios.c:854
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc1358
    je short 01377h                           ; 74 1a                       ; 0xc135b
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc135d
    je short 01372h                           ; 74 10                       ; 0xc1360
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc1362
    jne short 0137ch                          ; 75 15                       ; 0xc1365
    mov dx, 053f2h                            ; ba f2 53                    ; 0xc1367 vgabios.c:856
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc136a
    call 00a00h                               ; e8 90 f6                    ; 0xc136d
    jmp short 0137ch                          ; eb 0a                       ; 0xc1370 vgabios.c:857
    mov dx, 05bf2h                            ; ba f2 5b                    ; 0xc1372 vgabios.c:859
    jmp short 0136ah                          ; eb f3                       ; 0xc1375
    mov dx, 069f2h                            ; ba f2 69                    ; 0xc1377 vgabios.c:862
    jmp short 0136ah                          ; eb ee                       ; 0xc137a
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc137c vgabios.c:865
    pop di                                    ; 5f                          ; 0xc137f
    pop si                                    ; 5e                          ; 0xc1380
    pop dx                                    ; 5a                          ; 0xc1381
    pop cx                                    ; 59                          ; 0xc1382
    pop bx                                    ; 5b                          ; 0xc1383
    pop bp                                    ; 5d                          ; 0xc1384
    retn                                      ; c3                          ; 0xc1385
  ; disGetNextSymbol 0xc1386 LB 0x2753 -> off=0x0 cb=0000000000000076 uValue=00000000000c1386 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1386 LB 0x76
    push bp                                   ; 55                          ; 0xc1386 vgabios.c:868
    mov bp, sp                                ; 89 e5                       ; 0xc1387
    push si                                   ; 56                          ; 0xc1389
    push di                                   ; 57                          ; 0xc138a
    push ax                                   ; 50                          ; 0xc138b
    push ax                                   ; 50                          ; 0xc138c
    mov bh, cl                                ; 88 cf                       ; 0xc138d
    movzx di, dl                              ; 0f b6 fa                    ; 0xc138f vgabios.c:874
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1392
    imul di, cx                               ; 0f af f9                    ; 0xc1396
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc1399
    imul di, si                               ; 0f af fe                    ; 0xc139d
    xor ah, ah                                ; 30 e4                       ; 0xc13a0
    add di, ax                                ; 01 c7                       ; 0xc13a2
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc13a4
    movzx di, bl                              ; 0f b6 fb                    ; 0xc13a7 vgabios.c:875
    imul cx, di                               ; 0f af cf                    ; 0xc13aa
    imul cx, si                               ; 0f af ce                    ; 0xc13ad
    add cx, ax                                ; 01 c1                       ; 0xc13b0
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc13b2
    mov ax, 00105h                            ; b8 05 01                    ; 0xc13b5 vgabios.c:876
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc13b8
    out DX, ax                                ; ef                          ; 0xc13bb
    xor bl, bl                                ; 30 db                       ; 0xc13bc vgabios.c:877
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc13be
    jnc short 013ech                          ; 73 29                       ; 0xc13c1
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc13c3 vgabios.c:879
    movzx si, bl                              ; 0f b6 f3                    ; 0xc13c6
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc13c9
    imul ax, si                               ; 0f af c6                    ; 0xc13cd
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc13d0
    add si, ax                                ; 01 c6                       ; 0xc13d3
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc13d5
    add di, ax                                ; 01 c7                       ; 0xc13d8
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc13da
    mov es, dx                                ; 8e c2                       ; 0xc13dd
    cld                                       ; fc                          ; 0xc13df
    jcxz 013e8h                               ; e3 06                       ; 0xc13e0
    push DS                                   ; 1e                          ; 0xc13e2
    mov ds, dx                                ; 8e da                       ; 0xc13e3
    rep movsb                                 ; f3 a4                       ; 0xc13e5
    pop DS                                    ; 1f                          ; 0xc13e7
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc13e8 vgabios.c:880
    jmp short 013beh                          ; eb d2                       ; 0xc13ea
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc13ec vgabios.c:881
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc13ef
    out DX, ax                                ; ef                          ; 0xc13f2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc13f3 vgabios.c:882
    pop di                                    ; 5f                          ; 0xc13f6
    pop si                                    ; 5e                          ; 0xc13f7
    pop bp                                    ; 5d                          ; 0xc13f8
    retn 00004h                               ; c2 04 00                    ; 0xc13f9
  ; disGetNextSymbol 0xc13fc LB 0x26dd -> off=0x0 cb=0000000000000061 uValue=00000000000c13fc 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc13fc LB 0x61
    push bp                                   ; 55                          ; 0xc13fc vgabios.c:885
    mov bp, sp                                ; 89 e5                       ; 0xc13fd
    push di                                   ; 57                          ; 0xc13ff
    push ax                                   ; 50                          ; 0xc1400
    push ax                                   ; 50                          ; 0xc1401
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc1402
    mov bh, cl                                ; 88 cf                       ; 0xc1405
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc1407 vgabios.c:891
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc140a
    imul cx, dx                               ; 0f af ca                    ; 0xc140e
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc1411
    imul dx, cx                               ; 0f af d1                    ; 0xc1414
    xor ah, ah                                ; 30 e4                       ; 0xc1417
    add dx, ax                                ; 01 c2                       ; 0xc1419
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc141b
    mov ax, 00205h                            ; b8 05 02                    ; 0xc141e vgabios.c:892
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1421
    out DX, ax                                ; ef                          ; 0xc1424
    xor bl, bl                                ; 30 db                       ; 0xc1425 vgabios.c:893
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1427
    jnc short 0144eh                          ; 73 22                       ; 0xc142a
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc                 ; 0xc142c vgabios.c:895
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1430
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc1434
    movzx di, bh                              ; 0f b6 ff                    ; 0xc1437
    imul di, dx                               ; 0f af fa                    ; 0xc143a
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc143d
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1440
    mov es, dx                                ; 8e c2                       ; 0xc1443
    cld                                       ; fc                          ; 0xc1445
    jcxz 0144ah                               ; e3 02                       ; 0xc1446
    rep stosb                                 ; f3 aa                       ; 0xc1448
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc144a vgabios.c:896
    jmp short 01427h                          ; eb d9                       ; 0xc144c
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc144e vgabios.c:897
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1451
    out DX, ax                                ; ef                          ; 0xc1454
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc1455 vgabios.c:898
    pop di                                    ; 5f                          ; 0xc1458
    pop bp                                    ; 5d                          ; 0xc1459
    retn 00004h                               ; c2 04 00                    ; 0xc145a
  ; disGetNextSymbol 0xc145d LB 0x267c -> off=0x0 cb=00000000000000a4 uValue=00000000000c145d 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc145d LB 0xa4
    push bp                                   ; 55                          ; 0xc145d vgabios.c:901
    mov bp, sp                                ; 89 e5                       ; 0xc145e
    push si                                   ; 56                          ; 0xc1460
    push di                                   ; 57                          ; 0xc1461
    push ax                                   ; 50                          ; 0xc1462
    push ax                                   ; 50                          ; 0xc1463
    mov bh, cl                                ; 88 cf                       ; 0xc1464
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1466 vgabios.c:907
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1469
    imul di, cx                               ; 0f af f9                    ; 0xc146d
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc1470
    imul di, si                               ; 0f af fe                    ; 0xc1474
    sar di, 1                                 ; d1 ff                       ; 0xc1477
    xor ah, ah                                ; 30 e4                       ; 0xc1479
    add di, ax                                ; 01 c7                       ; 0xc147b
    mov word [bp-006h], di                    ; 89 7e fa                    ; 0xc147d
    movzx di, bl                              ; 0f b6 fb                    ; 0xc1480 vgabios.c:908
    imul cx, di                               ; 0f af cf                    ; 0xc1483
    imul si, cx                               ; 0f af f1                    ; 0xc1486
    sar si, 1                                 ; d1 fe                       ; 0xc1489
    add si, ax                                ; 01 c6                       ; 0xc148b
    mov word [bp-008h], si                    ; 89 76 f8                    ; 0xc148d
    xor bl, bl                                ; 30 db                       ; 0xc1490 vgabios.c:909
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1492
    jnc short 014f8h                          ; 73 61                       ; 0xc1495
    test bl, 001h                             ; f6 c3 01                    ; 0xc1497 vgabios.c:911
    je short 014cdh                           ; 74 31                       ; 0xc149a
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc149c vgabios.c:912
    movzx si, bl                              ; 0f b6 f3                    ; 0xc149f
    sar si, 1                                 ; d1 fe                       ; 0xc14a2
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc14a4
    imul ax, si                               ; 0f af c6                    ; 0xc14a8
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc14ab
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc14ae
    add si, ax                                ; 01 c6                       ; 0xc14b2
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc14b4
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc14b7
    add di, ax                                ; 01 c7                       ; 0xc14bb
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc14bd
    mov es, dx                                ; 8e c2                       ; 0xc14c0
    cld                                       ; fc                          ; 0xc14c2
    jcxz 014cbh                               ; e3 06                       ; 0xc14c3
    push DS                                   ; 1e                          ; 0xc14c5
    mov ds, dx                                ; 8e da                       ; 0xc14c6
    rep movsb                                 ; f3 a4                       ; 0xc14c8
    pop DS                                    ; 1f                          ; 0xc14ca
    jmp short 014f4h                          ; eb 27                       ; 0xc14cb vgabios.c:913
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc14cd vgabios.c:914
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc14d0
    sar ax, 1                                 ; d1 f8                       ; 0xc14d3
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc14d5
    imul ax, si                               ; 0f af c6                    ; 0xc14d9
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc14dc
    add si, ax                                ; 01 c6                       ; 0xc14df
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc14e1
    add di, ax                                ; 01 c7                       ; 0xc14e4
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc14e6
    mov es, dx                                ; 8e c2                       ; 0xc14e9
    cld                                       ; fc                          ; 0xc14eb
    jcxz 014f4h                               ; e3 06                       ; 0xc14ec
    push DS                                   ; 1e                          ; 0xc14ee
    mov ds, dx                                ; 8e da                       ; 0xc14ef
    rep movsb                                 ; f3 a4                       ; 0xc14f1
    pop DS                                    ; 1f                          ; 0xc14f3
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc14f4 vgabios.c:915
    jmp short 01492h                          ; eb 9a                       ; 0xc14f6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc14f8 vgabios.c:916
    pop di                                    ; 5f                          ; 0xc14fb
    pop si                                    ; 5e                          ; 0xc14fc
    pop bp                                    ; 5d                          ; 0xc14fd
    retn 00004h                               ; c2 04 00                    ; 0xc14fe
  ; disGetNextSymbol 0xc1501 LB 0x25d8 -> off=0x0 cb=000000000000008a uValue=00000000000c1501 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc1501 LB 0x8a
    push bp                                   ; 55                          ; 0xc1501 vgabios.c:919
    mov bp, sp                                ; 89 e5                       ; 0xc1502
    push si                                   ; 56                          ; 0xc1504
    push di                                   ; 57                          ; 0xc1505
    push ax                                   ; 50                          ; 0xc1506
    push ax                                   ; 50                          ; 0xc1507
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1508
    mov bh, cl                                ; 88 cf                       ; 0xc150b
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc150d vgabios.c:925
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1510
    imul dx, cx                               ; 0f af d1                    ; 0xc1514
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc1517
    imul dx, cx                               ; 0f af d1                    ; 0xc151a
    sar dx, 1                                 ; d1 fa                       ; 0xc151d
    movzx si, al                              ; 0f b6 f0                    ; 0xc151f
    add si, dx                                ; 01 d6                       ; 0xc1522
    xor bl, bl                                ; 30 db                       ; 0xc1524 vgabios.c:926
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1526
    jnc short 01582h                          ; 73 57                       ; 0xc1529
    test bl, 001h                             ; f6 c3 01                    ; 0xc152b vgabios.c:928
    je short 0155fh                           ; 74 2f                       ; 0xc152e
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa                 ; 0xc1530 vgabios.c:929
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1534
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc1538
    sar dx, 1                                 ; d1 fa                       ; 0xc153b
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc153d
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc1540
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc1543
    imul di, dx                               ; 0f af fa                    ; 0xc1546
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc1549
    lea di, [si+02000h]                       ; 8d bc 00 20                 ; 0xc154c
    add di, word [bp-008h]                    ; 03 7e f8                    ; 0xc1550
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1553
    mov es, dx                                ; 8e c2                       ; 0xc1556
    cld                                       ; fc                          ; 0xc1558
    jcxz 0155dh                               ; e3 02                       ; 0xc1559
    rep stosb                                 ; f3 aa                       ; 0xc155b
    jmp short 0157eh                          ; eb 1f                       ; 0xc155d vgabios.c:930
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa                 ; 0xc155f vgabios.c:931
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1563
    movzx di, bl                              ; 0f b6 fb                    ; 0xc1567
    sar di, 1                                 ; d1 ff                       ; 0xc156a
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc156c
    imul di, dx                               ; 0f af fa                    ; 0xc156f
    add di, si                                ; 01 f7                       ; 0xc1572
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1574
    mov es, dx                                ; 8e c2                       ; 0xc1577
    cld                                       ; fc                          ; 0xc1579
    jcxz 0157eh                               ; e3 02                       ; 0xc157a
    rep stosb                                 ; f3 aa                       ; 0xc157c
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc157e vgabios.c:932
    jmp short 01526h                          ; eb a4                       ; 0xc1580
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1582 vgabios.c:933
    pop di                                    ; 5f                          ; 0xc1585
    pop si                                    ; 5e                          ; 0xc1586
    pop bp                                    ; 5d                          ; 0xc1587
    retn 00004h                               ; c2 04 00                    ; 0xc1588
  ; disGetNextSymbol 0xc158b LB 0x254e -> off=0x0 cb=0000000000000506 uValue=00000000000c158b 'biosfn_scroll'
biosfn_scroll:                               ; 0xc158b LB 0x506
    push bp                                   ; 55                          ; 0xc158b vgabios.c:936
    mov bp, sp                                ; 89 e5                       ; 0xc158c
    push si                                   ; 56                          ; 0xc158e
    push di                                   ; 57                          ; 0xc158f
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1590
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1593
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc1596
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1599
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc159c
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc159f vgabios.c:945
    jnbe near 01a88h                          ; 0f 87 e2 04                 ; 0xc15a2
    cmp cl, byte [bp+006h]                    ; 3a 4e 06                    ; 0xc15a6 vgabios.c:946
    jnbe near 01a88h                          ; 0f 87 db 04                 ; 0xc15a9
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc15ad vgabios.c:949
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15b0
    call 02f57h                               ; e8 a1 19                    ; 0xc15b3
    xor ah, ah                                ; 30 e4                       ; 0xc15b6 vgabios.c:950
    call 02f30h                               ; e8 75 19                    ; 0xc15b8
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc15bb
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc15be vgabios.c:951
    je near 01a88h                            ; 0f 84 c4 04                 ; 0xc15c0
    mov dx, 00084h                            ; ba 84 00                    ; 0xc15c4 vgabios.c:954
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15c7
    call 02f57h                               ; e8 8a 19                    ; 0xc15ca
    movzx cx, al                              ; 0f b6 c8                    ; 0xc15cd
    inc cx                                    ; 41                          ; 0xc15d0
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc15d1 vgabios.c:955
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15d4
    call 02f73h                               ; e8 99 19                    ; 0xc15d7
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc15da
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc15dd vgabios.c:958
    jne short 015efh                          ; 75 0c                       ; 0xc15e1
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc15e3 vgabios.c:959
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15e6
    call 02f57h                               ; e8 6b 19                    ; 0xc15e9
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc15ec
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc15ef vgabios.c:961
    cmp ax, cx                                ; 39 c8                       ; 0xc15f3
    jc short 015feh                           ; 72 07                       ; 0xc15f5
    mov al, cl                                ; 88 c8                       ; 0xc15f7
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc15f9
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc15fb
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc15fe vgabios.c:962
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1602
    jc short 0160fh                           ; 72 08                       ; 0xc1605
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1607
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc160a
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc160c
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc160f vgabios.c:963
    cmp ax, cx                                ; 39 c8                       ; 0xc1613
    jbe short 0161bh                          ; 76 04                       ; 0xc1615
    mov byte [bp-010h], 000h                  ; c6 46 f0 00                 ; 0xc1617
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc161b vgabios.c:964
    sub al, byte [bp-006h]                    ; 2a 46 fa                    ; 0xc161e
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1621
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1623
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2                 ; 0xc1626 vgabios.c:966
    mov di, si                                ; 89 f7                       ; 0xc162a
    sal di, 003h                              ; c1 e7 03                    ; 0xc162c
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc162f
    dec ax                                    ; 48                          ; 0xc1632
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1633
    mov ax, cx                                ; 89 c8                       ; 0xc1636
    dec ax                                    ; 48                          ; 0xc1638
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1639
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc163c
    imul ax, cx                               ; 0f af c1                    ; 0xc163f
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1642
    jne near 017eah                           ; 0f 85 9f 01                 ; 0xc1647
    mov dx, ax                                ; 89 c2                       ; 0xc164b vgabios.c:969
    add dx, ax                                ; 01 c2                       ; 0xc164d
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc164f
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08                 ; 0xc1652
    inc dx                                    ; 42                          ; 0xc1656
    imul bx, dx                               ; 0f af da                    ; 0xc1657
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc165a vgabios.c:974
    jne short 0169ah                          ; 75 3a                       ; 0xc165e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1660
    jne short 0169ah                          ; 75 34                       ; 0xc1664
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1666
    jne short 0169ah                          ; 75 2e                       ; 0xc166a
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc166c
    cmp dx, word [bp-01ah]                    ; 3b 56 e6                    ; 0xc1670
    jne short 0169ah                          ; 75 25                       ; 0xc1673
    movzx dx, byte [bp+006h]                  ; 0f b6 56 06                 ; 0xc1675
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1679
    jne short 0169ah                          ; 75 1c                       ; 0xc167c
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc167e vgabios.c:976
    sal dx, 008h                              ; c1 e2 08                    ; 0xc1682
    add dx, strict byte 00020h                ; 83 c2 20                    ; 0xc1685
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1688
    mov cx, ax                                ; 89 c1                       ; 0xc168c
    mov ax, dx                                ; 89 d0                       ; 0xc168e
    mov di, bx                                ; 89 df                       ; 0xc1690
    cld                                       ; fc                          ; 0xc1692
    jcxz 01697h                               ; e3 02                       ; 0xc1693
    rep stosw                                 ; f3 ab                       ; 0xc1695
    jmp near 01a88h                           ; e9 ee 03                    ; 0xc1697 vgabios.c:978
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc169a vgabios.c:980
    jne near 0173fh                           ; 0f 85 9d 00                 ; 0xc169e
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc16a2 vgabios.c:981
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc16a6
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc16a9
    cmp dx, word [bp-014h]                    ; 3b 56 ec                    ; 0xc16ad
    jc near 01a88h                            ; 0f 82 d4 03                 ; 0xc16b0
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc16b4 vgabios.c:983
    add ax, word [bp-014h]                    ; 03 46 ec                    ; 0xc16b8
    cmp ax, dx                                ; 39 d0                       ; 0xc16bb
    jnbe short 016c5h                         ; 77 06                       ; 0xc16bd
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc16bf
    jne short 016f8h                          ; 75 33                       ; 0xc16c3
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc16c5 vgabios.c:984
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc16c9
    sal ax, 008h                              ; c1 e0 08                    ; 0xc16cd
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc16d0
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc16d3
    imul si, word [bp-016h]                   ; 0f af 76 ea                 ; 0xc16d6
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc16da
    add dx, si                                ; 01 f2                       ; 0xc16de
    add dx, dx                                ; 01 d2                       ; 0xc16e0
    mov di, bx                                ; 89 df                       ; 0xc16e2
    add di, dx                                ; 01 d7                       ; 0xc16e4
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2                 ; 0xc16e6
    sal si, 003h                              ; c1 e6 03                    ; 0xc16ea
    mov es, [si+04638h]                       ; 8e 84 38 46                 ; 0xc16ed
    cld                                       ; fc                          ; 0xc16f1
    jcxz 016f6h                               ; e3 02                       ; 0xc16f2
    rep stosw                                 ; f3 ab                       ; 0xc16f4
    jmp short 01739h                          ; eb 41                       ; 0xc16f6 vgabios.c:985
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc16f8 vgabios.c:986
    mov word [bp-01ch], dx                    ; 89 56 e4                    ; 0xc16fc
    mov dx, ax                                ; 89 c2                       ; 0xc16ff
    imul dx, word [bp-016h]                   ; 0f af 56 ea                 ; 0xc1701
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa                 ; 0xc1705
    add dx, cx                                ; 01 ca                       ; 0xc1709
    add dx, dx                                ; 01 d2                       ; 0xc170b
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2                 ; 0xc170d
    sal si, 003h                              ; c1 e6 03                    ; 0xc1711
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc1714
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc1718
    imul si, word [bp-016h]                   ; 0f af 76 ea                 ; 0xc171b
    add cx, si                                ; 01 f1                       ; 0xc171f
    add cx, cx                                ; 01 c9                       ; 0xc1721
    mov di, bx                                ; 89 df                       ; 0xc1723
    add di, cx                                ; 01 cf                       ; 0xc1725
    mov cx, word [bp-01ch]                    ; 8b 4e e4                    ; 0xc1727
    mov si, dx                                ; 89 d6                       ; 0xc172a
    mov dx, ax                                ; 89 c2                       ; 0xc172c
    mov es, ax                                ; 8e c0                       ; 0xc172e
    cld                                       ; fc                          ; 0xc1730
    jcxz 01739h                               ; e3 06                       ; 0xc1731
    push DS                                   ; 1e                          ; 0xc1733
    mov ds, dx                                ; 8e da                       ; 0xc1734
    rep movsw                                 ; f3 a5                       ; 0xc1736
    pop DS                                    ; 1f                          ; 0xc1738
    inc word [bp-014h]                        ; ff 46 ec                    ; 0xc1739 vgabios.c:987
    jmp near 016a9h                           ; e9 6a ff                    ; 0xc173c
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc173f vgabios.c:990
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1743
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1746
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc174a
    jnbe near 01a88h                          ; 0f 87 37 03                 ; 0xc174d
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1751 vgabios.c:992
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1755
    add ax, dx                                ; 01 d0                       ; 0xc1759
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc175b
    jnbe short 01766h                         ; 77 06                       ; 0xc175e
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1760
    jne short 01799h                          ; 75 33                       ; 0xc1764
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc1766 vgabios.c:993
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc176a
    sal ax, 008h                              ; c1 e0 08                    ; 0xc176e
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1771
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc1774
    imul si, word [bp-016h]                   ; 0f af 76 ea                 ; 0xc1777
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc177b
    add dx, si                                ; 01 f2                       ; 0xc177f
    add dx, dx                                ; 01 d2                       ; 0xc1781
    mov di, bx                                ; 89 df                       ; 0xc1783
    add di, dx                                ; 01 d7                       ; 0xc1785
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2                 ; 0xc1787
    sal si, 003h                              ; c1 e6 03                    ; 0xc178b
    mov es, [si+04638h]                       ; 8e 84 38 46                 ; 0xc178e
    cld                                       ; fc                          ; 0xc1792
    jcxz 01797h                               ; e3 02                       ; 0xc1793
    rep stosw                                 ; f3 ab                       ; 0xc1795
    jmp short 017d9h                          ; eb 40                       ; 0xc1797 vgabios.c:994
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc1799 vgabios.c:995
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc179d
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc17a1
    sub dx, ax                                ; 29 c2                       ; 0xc17a4
    imul dx, word [bp-016h]                   ; 0f af 56 ea                 ; 0xc17a6
    movzx di, byte [bp-006h]                  ; 0f b6 7e fa                 ; 0xc17aa
    add dx, di                                ; 01 fa                       ; 0xc17ae
    add dx, dx                                ; 01 d2                       ; 0xc17b0
    movzx si, byte [bp-00eh]                  ; 0f b6 76 f2                 ; 0xc17b2
    sal si, 003h                              ; c1 e6 03                    ; 0xc17b6
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc17b9
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc17bd
    imul si, word [bp-016h]                   ; 0f af 76 ea                 ; 0xc17c0
    add di, si                                ; 01 f7                       ; 0xc17c4
    add di, di                                ; 01 ff                       ; 0xc17c6
    add di, bx                                ; 01 df                       ; 0xc17c8
    mov si, dx                                ; 89 d6                       ; 0xc17ca
    mov dx, ax                                ; 89 c2                       ; 0xc17cc
    mov es, ax                                ; 8e c0                       ; 0xc17ce
    cld                                       ; fc                          ; 0xc17d0
    jcxz 017d9h                               ; e3 06                       ; 0xc17d1
    push DS                                   ; 1e                          ; 0xc17d3
    mov ds, dx                                ; 8e da                       ; 0xc17d4
    rep movsw                                 ; f3 a5                       ; 0xc17d6
    pop DS                                    ; 1f                          ; 0xc17d8
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc17d9 vgabios.c:996
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc17dd
    jc near 01a88h                            ; 0f 82 a4 02                 ; 0xc17e0
    dec word [bp-014h]                        ; ff 4e ec                    ; 0xc17e4 vgabios.c:997
    jmp near 01746h                           ; e9 5c ff                    ; 0xc17e7
    movzx bx, byte [si+046b4h]                ; 0f b6 9c b4 46              ; 0xc17ea vgabios.c:1004
    sal bx, 006h                              ; c1 e3 06                    ; 0xc17ef
    mov dl, byte [bx+046cah]                  ; 8a 97 ca 46                 ; 0xc17f2
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc17f6
    mov bl, byte [di+04636h]                  ; 8a 9d 36 46                 ; 0xc17f9 vgabios.c:1005
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc17fd
    je short 01811h                           ; 74 0f                       ; 0xc1800
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1802
    je short 01811h                           ; 74 0a                       ; 0xc1805
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1807
    je near 01950h                            ; 0f 84 42 01                 ; 0xc180a
    jmp near 01a88h                           ; e9 77 02                    ; 0xc180e
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1811 vgabios.c:1009
    jne short 01869h                          ; 75 52                       ; 0xc1815
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1817
    jne short 01869h                          ; 75 4c                       ; 0xc181b
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc181d
    jne short 01869h                          ; 75 46                       ; 0xc1821
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1823
    mov ax, cx                                ; 89 c8                       ; 0xc1827
    dec ax                                    ; 48                          ; 0xc1829
    cmp dx, ax                                ; 39 c2                       ; 0xc182a
    jne short 01869h                          ; 75 3b                       ; 0xc182c
    movzx dx, byte [bp+006h]                  ; 0f b6 56 06                 ; 0xc182e
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1832
    dec ax                                    ; 48                          ; 0xc1835
    cmp dx, ax                                ; 39 c2                       ; 0xc1836
    jne short 01869h                          ; 75 2f                       ; 0xc1838
    mov ax, 00205h                            ; b8 05 02                    ; 0xc183a vgabios.c:1011
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc183d
    out DX, ax                                ; ef                          ; 0xc1840
    imul cx, word [bp-016h]                   ; 0f af 4e ea                 ; 0xc1841 vgabios.c:1012
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1845
    imul cx, ax                               ; 0f af c8                    ; 0xc1849
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc184c
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc1850
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1854
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1857
    xor di, di                                ; 31 ff                       ; 0xc185b
    cld                                       ; fc                          ; 0xc185d
    jcxz 01862h                               ; e3 02                       ; 0xc185e
    rep stosb                                 ; f3 aa                       ; 0xc1860
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1862 vgabios.c:1013
    out DX, ax                                ; ef                          ; 0xc1865
    jmp near 01a88h                           ; e9 1f 02                    ; 0xc1866 vgabios.c:1015
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1869 vgabios.c:1017
    jne short 018d8h                          ; 75 69                       ; 0xc186d
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc186f vgabios.c:1018
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1873
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1876
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc187a
    jc near 01a88h                            ; 0f 82 07 02                 ; 0xc187d
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1881 vgabios.c:1020
    add dx, word [bp-014h]                    ; 03 56 ec                    ; 0xc1885
    cmp dx, ax                                ; 39 c2                       ; 0xc1888
    jnbe short 01892h                         ; 77 06                       ; 0xc188a
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc188c
    jne short 018b1h                          ; 75 1f                       ; 0xc1890
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1892 vgabios.c:1021
    push ax                                   ; 50                          ; 0xc1896
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1897
    push ax                                   ; 50                          ; 0xc189b
    movzx cx, byte [bp-016h]                  ; 0f b6 4e ea                 ; 0xc189c
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc18a0
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc18a4
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc18a8
    call 013fch                               ; e8 4d fb                    ; 0xc18ac
    jmp short 018d3h                          ; eb 22                       ; 0xc18af vgabios.c:1022
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc18b1 vgabios.c:1023
    push ax                                   ; 50                          ; 0xc18b5
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc18b6
    push ax                                   ; 50                          ; 0xc18ba
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc18bb
    movzx bx, byte [bp-014h]                  ; 0f b6 5e ec                 ; 0xc18bf
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc18c3
    add al, byte [bp-010h]                    ; 02 46 f0                    ; 0xc18c6
    movzx dx, al                              ; 0f b6 d0                    ; 0xc18c9
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc18cc
    call 01386h                               ; e8 b3 fa                    ; 0xc18d0
    inc word [bp-014h]                        ; ff 46 ec                    ; 0xc18d3 vgabios.c:1024
    jmp short 01876h                          ; eb 9e                       ; 0xc18d6
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc18d8 vgabios.c:1027
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc18dc
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc18df
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc18e3
    jnbe near 01a88h                          ; 0f 87 9e 01                 ; 0xc18e6
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc18ea vgabios.c:1029
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc18ee
    add ax, dx                                ; 01 d0                       ; 0xc18f2
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc18f4
    jnbe short 018ffh                         ; 77 06                       ; 0xc18f7
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc18f9
    jne short 0191eh                          ; 75 1f                       ; 0xc18fd
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc18ff vgabios.c:1030
    push ax                                   ; 50                          ; 0xc1903
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1904
    push ax                                   ; 50                          ; 0xc1908
    movzx cx, byte [bp-016h]                  ; 0f b6 4e ea                 ; 0xc1909
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc190d
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1911
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1915
    call 013fch                               ; e8 e0 fa                    ; 0xc1919
    jmp short 01940h                          ; eb 22                       ; 0xc191c vgabios.c:1031
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc191e vgabios.c:1032
    push ax                                   ; 50                          ; 0xc1922
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1923
    push ax                                   ; 50                          ; 0xc1927
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc1928
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc192c
    sub al, byte [bp-010h]                    ; 2a 46 f0                    ; 0xc192f
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1932
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1935
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1939
    call 01386h                               ; e8 46 fa                    ; 0xc193d
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1940 vgabios.c:1033
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc1944
    jc near 01a88h                            ; 0f 82 3d 01                 ; 0xc1947
    dec word [bp-014h]                        ; ff 4e ec                    ; 0xc194b vgabios.c:1034
    jmp short 018dfh                          ; eb 8f                       ; 0xc194e
    mov dl, byte [di+04637h]                  ; 8a 95 37 46                 ; 0xc1950 vgabios.c:1039
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1954 vgabios.c:1040
    jne short 01997h                          ; 75 3d                       ; 0xc1958
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc195a
    jne short 01997h                          ; 75 37                       ; 0xc195e
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1960
    jne short 01997h                          ; 75 31                       ; 0xc1964
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1966
    cmp bx, word [bp-01ah]                    ; 3b 5e e6                    ; 0xc196a
    jne short 01997h                          ; 75 28                       ; 0xc196d
    movzx bx, byte [bp+006h]                  ; 0f b6 5e 06                 ; 0xc196f
    cmp bx, word [bp-018h]                    ; 3b 5e e8                    ; 0xc1973
    jne short 01997h                          ; 75 1f                       ; 0xc1976
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1978 vgabios.c:1042
    imul ax, bx                               ; 0f af c3                    ; 0xc197c
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc197f
    imul cx, ax                               ; 0f af c8                    ; 0xc1982
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1985
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1989
    xor di, di                                ; 31 ff                       ; 0xc198d
    cld                                       ; fc                          ; 0xc198f
    jcxz 01994h                               ; e3 02                       ; 0xc1990
    rep stosb                                 ; f3 aa                       ; 0xc1992
    jmp near 01a88h                           ; e9 f1 00                    ; 0xc1994 vgabios.c:1044
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1997 vgabios.c:1046
    jne short 019a5h                          ; 75 09                       ; 0xc199a
    sal byte [bp-006h], 1                     ; d0 66 fa                    ; 0xc199c vgabios.c:1048
    sal byte [bp-012h], 1                     ; d0 66 ee                    ; 0xc199f vgabios.c:1049
    sal word [bp-016h], 1                     ; d1 66 ea                    ; 0xc19a2 vgabios.c:1050
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc19a5 vgabios.c:1053
    jne short 01a14h                          ; 75 69                       ; 0xc19a9
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc19ab vgabios.c:1054
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc19af
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc19b2
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc19b6
    jc near 01a88h                            ; 0f 82 cb 00                 ; 0xc19b9
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc19bd vgabios.c:1056
    add dx, word [bp-014h]                    ; 03 56 ec                    ; 0xc19c1
    cmp dx, ax                                ; 39 c2                       ; 0xc19c4
    jnbe short 019ceh                         ; 77 06                       ; 0xc19c6
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc19c8
    jne short 019edh                          ; 75 1f                       ; 0xc19cc
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc19ce vgabios.c:1057
    push ax                                   ; 50                          ; 0xc19d2
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc19d3
    push ax                                   ; 50                          ; 0xc19d7
    movzx cx, byte [bp-016h]                  ; 0f b6 4e ea                 ; 0xc19d8
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc19dc
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc19e0
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc19e4
    call 01501h                               ; e8 16 fb                    ; 0xc19e8
    jmp short 01a0fh                          ; eb 22                       ; 0xc19eb vgabios.c:1058
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc19ed vgabios.c:1059
    push ax                                   ; 50                          ; 0xc19f1
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc19f2
    push ax                                   ; 50                          ; 0xc19f6
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc19f7
    movzx bx, byte [bp-014h]                  ; 0f b6 5e ec                 ; 0xc19fb
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc19ff
    add al, byte [bp-010h]                    ; 02 46 f0                    ; 0xc1a02
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1a05
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1a08
    call 0145dh                               ; e8 4e fa                    ; 0xc1a0c
    inc word [bp-014h]                        ; ff 46 ec                    ; 0xc1a0f vgabios.c:1060
    jmp short 019b2h                          ; eb 9e                       ; 0xc1a12
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1a14 vgabios.c:1063
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1a18
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1a1b
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc1a1f
    jnbe short 01a88h                         ; 77 64                       ; 0xc1a22
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1a24 vgabios.c:1065
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1a28
    add ax, dx                                ; 01 d0                       ; 0xc1a2c
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc1a2e
    jnbe short 01a39h                         ; 77 06                       ; 0xc1a31
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1a33
    jne short 01a58h                          ; 75 1f                       ; 0xc1a37
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1a39 vgabios.c:1066
    push ax                                   ; 50                          ; 0xc1a3d
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1a3e
    push ax                                   ; 50                          ; 0xc1a42
    movzx cx, byte [bp-016h]                  ; 0f b6 4e ea                 ; 0xc1a43
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1a47
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1a4b
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1a4f
    call 01501h                               ; e8 ab fa                    ; 0xc1a53
    jmp short 01a7ah                          ; eb 22                       ; 0xc1a56 vgabios.c:1067
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1a58 vgabios.c:1068
    push ax                                   ; 50                          ; 0xc1a5c
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1a5d
    push ax                                   ; 50                          ; 0xc1a61
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee                 ; 0xc1a62
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc1a66
    sub al, byte [bp-010h]                    ; 2a 46 f0                    ; 0xc1a69
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1a6c
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1a6f
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1a73
    call 0145dh                               ; e8 e3 f9                    ; 0xc1a77
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1a7a vgabios.c:1069
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc1a7e
    jc short 01a88h                           ; 72 05                       ; 0xc1a81
    dec word [bp-014h]                        ; ff 4e ec                    ; 0xc1a83 vgabios.c:1070
    jmp short 01a1bh                          ; eb 93                       ; 0xc1a86
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a88 vgabios.c:1081
    pop di                                    ; 5f                          ; 0xc1a8b
    pop si                                    ; 5e                          ; 0xc1a8c
    pop bp                                    ; 5d                          ; 0xc1a8d
    retn 00008h                               ; c2 08 00                    ; 0xc1a8e
  ; disGetNextSymbol 0xc1a91 LB 0x2048 -> off=0x0 cb=00000000000000eb uValue=00000000000c1a91 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc1a91 LB 0xeb
    push bp                                   ; 55                          ; 0xc1a91 vgabios.c:1084
    mov bp, sp                                ; 89 e5                       ; 0xc1a92
    push si                                   ; 56                          ; 0xc1a94
    push di                                   ; 57                          ; 0xc1a95
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1a96
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc1a99
    mov ah, bl                                ; 88 dc                       ; 0xc1a9c
    cmp byte [bp+006h], 010h                  ; 80 7e 06 10                 ; 0xc1a9e vgabios.c:1091
    je short 01aafh                           ; 74 0b                       ; 0xc1aa2
    cmp byte [bp+006h], 00eh                  ; 80 7e 06 0e                 ; 0xc1aa4
    jne short 01ab4h                          ; 75 0a                       ; 0xc1aa8
    mov di, 05bf2h                            ; bf f2 5b                    ; 0xc1aaa vgabios.c:1093
    jmp short 01ab7h                          ; eb 08                       ; 0xc1aad vgabios.c:1094
    mov di, 069f2h                            ; bf f2 69                    ; 0xc1aaf vgabios.c:1096
    jmp short 01ab7h                          ; eb 03                       ; 0xc1ab2 vgabios.c:1097
    mov di, 053f2h                            ; bf f2 53                    ; 0xc1ab4 vgabios.c:1099
    movzx si, cl                              ; 0f b6 f1                    ; 0xc1ab7 vgabios.c:1101
    movzx bx, byte [bp+006h]                  ; 0f b6 5e 06                 ; 0xc1aba
    imul si, bx                               ; 0f af f3                    ; 0xc1abe
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1ac1
    imul cx, si                               ; 0f af ce                    ; 0xc1ac5
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1ac8
    add si, cx                                ; 01 ce                       ; 0xc1acb
    mov word [bp-00eh], si                    ; 89 76 f2                    ; 0xc1acd
    xor ah, ah                                ; 30 e4                       ; 0xc1ad0 vgabios.c:1102
    imul ax, bx                               ; 0f af c3                    ; 0xc1ad2
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1ad5
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc1ad8 vgabios.c:1103
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1adb
    out DX, ax                                ; ef                          ; 0xc1ade
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1adf vgabios.c:1104
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1ae2
    out DX, ax                                ; ef                          ; 0xc1ae5
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc1ae6 vgabios.c:1105
    je short 01af2h                           ; 74 06                       ; 0xc1aea
    mov ax, 01803h                            ; b8 03 18                    ; 0xc1aec vgabios.c:1107
    out DX, ax                                ; ef                          ; 0xc1aef
    jmp short 01af6h                          ; eb 04                       ; 0xc1af0 vgabios.c:1109
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1af2 vgabios.c:1111
    out DX, ax                                ; ef                          ; 0xc1af5
    xor ch, ch                                ; 30 ed                       ; 0xc1af6 vgabios.c:1113
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc1af8
    jnc short 01b64h                          ; 73 67                       ; 0xc1afb
    movzx si, ch                              ; 0f b6 f5                    ; 0xc1afd vgabios.c:1115
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1b00
    imul si, ax                               ; 0f af f0                    ; 0xc1b04
    add si, word [bp-00eh]                    ; 03 76 f2                    ; 0xc1b07
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1b0a vgabios.c:1116
    jmp short 01b23h                          ; eb 13                       ; 0xc1b0e
    xor bx, bx                                ; 31 db                       ; 0xc1b10 vgabios.c:1127
    mov dx, si                                ; 89 f2                       ; 0xc1b12
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1b14
    call 02f65h                               ; e8 4b 14                    ; 0xc1b17
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1b1a vgabios.c:1129
    cmp byte [bp-008h], 008h                  ; 80 7e f8 08                 ; 0xc1b1d
    jnc short 01b60h                          ; 73 3d                       ; 0xc1b21
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1b23
    mov cl, al                                ; 88 c1                       ; 0xc1b27
    mov ax, 00080h                            ; b8 80 00                    ; 0xc1b29
    sar ax, CL                                ; d3 f8                       ; 0xc1b2c
    xor ah, ah                                ; 30 e4                       ; 0xc1b2e
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1b30
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1b33
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc1b36
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1b38
    out DX, ax                                ; ef                          ; 0xc1b3b
    mov dx, si                                ; 89 f2                       ; 0xc1b3c
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1b3e
    call 02f57h                               ; e8 13 14                    ; 0xc1b41
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc1b44
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc1b47
    mov bx, di                                ; 89 fb                       ; 0xc1b4a
    add bx, ax                                ; 01 c3                       ; 0xc1b4c
    movzx ax, byte [bx]                       ; 0f b6 07                    ; 0xc1b4e
    test word [bp-00ch], ax                   ; 85 46 f4                    ; 0xc1b51
    je short 01b10h                           ; 74 ba                       ; 0xc1b54
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b56
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc1b59
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1b5b
    jmp short 01b12h                          ; eb b2                       ; 0xc1b5e
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1b60 vgabios.c:1130
    jmp short 01af8h                          ; eb 94                       ; 0xc1b62
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc1b64 vgabios.c:1131
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1b67
    out DX, ax                                ; ef                          ; 0xc1b6a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1b6b vgabios.c:1132
    out DX, ax                                ; ef                          ; 0xc1b6e
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1b6f vgabios.c:1133
    out DX, ax                                ; ef                          ; 0xc1b72
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1b73 vgabios.c:1134
    pop di                                    ; 5f                          ; 0xc1b76
    pop si                                    ; 5e                          ; 0xc1b77
    pop bp                                    ; 5d                          ; 0xc1b78
    retn 00004h                               ; c2 04 00                    ; 0xc1b79
  ; disGetNextSymbol 0xc1b7c LB 0x1f5d -> off=0x0 cb=000000000000011e uValue=00000000000c1b7c 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc1b7c LB 0x11e
    push bp                                   ; 55                          ; 0xc1b7c vgabios.c:1137
    mov bp, sp                                ; 89 e5                       ; 0xc1b7d
    push si                                   ; 56                          ; 0xc1b7f
    push di                                   ; 57                          ; 0xc1b80
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1b81
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1b84
    mov si, 053f2h                            ; be f2 53                    ; 0xc1b87 vgabios.c:1144
    xor bh, bh                                ; 30 ff                       ; 0xc1b8a vgabios.c:1145
    movzx di, byte [bp+006h]                  ; 0f b6 7e 06                 ; 0xc1b8c
    imul di, bx                               ; 0f af fb                    ; 0xc1b90
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc1b93
    imul bx, bx, 00140h                       ; 69 db 40 01                 ; 0xc1b96
    add di, bx                                ; 01 df                       ; 0xc1b9a
    mov word [bp-00ch], di                    ; 89 7e f4                    ; 0xc1b9c
    movzx di, al                              ; 0f b6 f8                    ; 0xc1b9f vgabios.c:1146
    sal di, 003h                              ; c1 e7 03                    ; 0xc1ba2
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1ba5 vgabios.c:1147
    jmp near 01bfch                           ; e9 50 00                    ; 0xc1ba9
    xor al, al                                ; 30 c0                       ; 0xc1bac vgabios.c:1160
    xor ah, ah                                ; 30 e4                       ; 0xc1bae vgabios.c:1162
    jmp short 01bbdh                          ; eb 0b                       ; 0xc1bb0
    or al, bl                                 ; 08 d8                       ; 0xc1bb2 vgabios.c:1172
    shr ch, 1                                 ; d0 ed                       ; 0xc1bb4 vgabios.c:1175
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1bb6 vgabios.c:1176
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc1bb8
    jnc short 01be5h                          ; 73 28                       ; 0xc1bbb
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1bbd
    add bx, di                                ; 01 fb                       ; 0xc1bc1
    add bx, si                                ; 01 f3                       ; 0xc1bc3
    movzx bx, byte [bx]                       ; 0f b6 1f                    ; 0xc1bc5
    movzx dx, ch                              ; 0f b6 d5                    ; 0xc1bc8
    test bx, dx                               ; 85 d3                       ; 0xc1bcb
    je short 01bb4h                           ; 74 e5                       ; 0xc1bcd
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc1bcf
    sub cl, ah                                ; 28 e1                       ; 0xc1bd1
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1bd3
    and bl, 001h                              ; 80 e3 01                    ; 0xc1bd6
    sal bl, CL                                ; d2 e3                       ; 0xc1bd9
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1bdb
    je short 01bb2h                           ; 74 d1                       ; 0xc1bdf
    xor al, bl                                ; 30 d8                       ; 0xc1be1
    jmp short 01bb4h                          ; eb cf                       ; 0xc1be3
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1be5 vgabios.c:1177
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc1be8
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1beb
    call 02f65h                               ; e8 74 13                    ; 0xc1bee
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1bf1 vgabios.c:1179
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc1bf4
    jnc near 01c91h                           ; 0f 83 95 00                 ; 0xc1bf8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1bfc
    sar ax, 1                                 ; d1 f8                       ; 0xc1c00
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc1c02
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1c05
    add bx, ax                                ; 01 c3                       ; 0xc1c08
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc1c0a
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc1c0d
    je short 01c17h                           ; 74 04                       ; 0xc1c11
    add byte [bp-009h], 020h                  ; 80 46 f7 20                 ; 0xc1c13
    mov CH, strict byte 080h                  ; b5 80                       ; 0xc1c17
    cmp byte [bp+006h], 001h                  ; 80 7e 06 01                 ; 0xc1c19
    jne short 01c30h                          ; 75 11                       ; 0xc1c1d
    test byte [bp-008h], ch                   ; 84 6e f8                    ; 0xc1c1f
    je short 01bach                           ; 74 88                       ; 0xc1c22
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc1c24
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1c27
    call 02f57h                               ; e8 2a 13                    ; 0xc1c2a
    jmp near 01baeh                           ; e9 7e ff                    ; 0xc1c2d
    test ch, ch                               ; 84 ed                       ; 0xc1c30 vgabios.c:1181
    jbe short 01bf1h                          ; 76 bd                       ; 0xc1c32
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1c34 vgabios.c:1183
    je short 01c45h                           ; 74 0b                       ; 0xc1c38
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc1c3a vgabios.c:1185
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1c3d
    call 02f57h                               ; e8 14 13                    ; 0xc1c40
    jmp short 01c47h                          ; eb 02                       ; 0xc1c43 vgabios.c:1187
    xor al, al                                ; 30 c0                       ; 0xc1c45 vgabios.c:1189
    xor ah, ah                                ; 30 e4                       ; 0xc1c47 vgabios.c:1191
    jmp short 01c56h                          ; eb 0b                       ; 0xc1c49
    or al, bl                                 ; 08 d8                       ; 0xc1c4b vgabios.c:1201
    shr ch, 1                                 ; d0 ed                       ; 0xc1c4d vgabios.c:1204
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1c4f vgabios.c:1205
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc1c51
    jnc short 01c80h                          ; 73 2a                       ; 0xc1c54
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1c56
    add bx, di                                ; 01 fb                       ; 0xc1c5a
    add bx, si                                ; 01 f3                       ; 0xc1c5c
    movzx dx, byte [bx]                       ; 0f b6 17                    ; 0xc1c5e
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1c61
    test bx, dx                               ; 85 d3                       ; 0xc1c64
    je short 01c4dh                           ; 74 e5                       ; 0xc1c66
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1c68
    sub cl, ah                                ; 28 e1                       ; 0xc1c6a
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1c6c
    and bl, 003h                              ; 80 e3 03                    ; 0xc1c6f
    add cl, cl                                ; 00 c9                       ; 0xc1c72
    sal bl, CL                                ; d2 e3                       ; 0xc1c74
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1c76
    je short 01c4bh                           ; 74 cf                       ; 0xc1c7a
    xor al, bl                                ; 30 d8                       ; 0xc1c7c
    jmp short 01c4dh                          ; eb cd                       ; 0xc1c7e
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1c80 vgabios.c:1206
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc1c83
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1c86
    call 02f65h                               ; e8 d9 12                    ; 0xc1c89
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc1c8c vgabios.c:1207
    jmp short 01c30h                          ; eb 9f                       ; 0xc1c8f vgabios.c:1208
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1c91 vgabios.c:1211
    pop di                                    ; 5f                          ; 0xc1c94
    pop si                                    ; 5e                          ; 0xc1c95
    pop bp                                    ; 5d                          ; 0xc1c96
    retn 00004h                               ; c2 04 00                    ; 0xc1c97
  ; disGetNextSymbol 0xc1c9a LB 0x1e3f -> off=0x0 cb=0000000000000091 uValue=00000000000c1c9a 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc1c9a LB 0x91
    push bp                                   ; 55                          ; 0xc1c9a vgabios.c:1214
    mov bp, sp                                ; 89 e5                       ; 0xc1c9b
    push si                                   ; 56                          ; 0xc1c9d
    push di                                   ; 57                          ; 0xc1c9e
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1c9f
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc1ca2
    mov di, 053f2h                            ; bf f2 53                    ; 0xc1ca5 vgabios.c:1221
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc1ca8 vgabios.c:1222
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1cab
    imul cx, dx                               ; 0f af ca                    ; 0xc1caf
    sal cx, 006h                              ; c1 e1 06                    ; 0xc1cb2
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc1cb5
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1cb8
    add dx, cx                                ; 01 ca                       ; 0xc1cbb
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc1cbd
    movzx si, al                              ; 0f b6 f0                    ; 0xc1cc0 vgabios.c:1223
    sal si, 003h                              ; c1 e6 03                    ; 0xc1cc3
    xor cl, cl                                ; 30 c9                       ; 0xc1cc6 vgabios.c:1224
    jmp short 01d05h                          ; eb 3b                       ; 0xc1cc8
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc1cca vgabios.c:1228
    jnc short 01cfeh                          ; 73 2f                       ; 0xc1ccd
    xor al, al                                ; 30 c0                       ; 0xc1ccf vgabios.c:1230
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc1cd1 vgabios.c:1231
    add dx, si                                ; 01 f2                       ; 0xc1cd4
    mov bx, di                                ; 89 fb                       ; 0xc1cd6
    add bx, dx                                ; 01 d3                       ; 0xc1cd8
    movzx dx, byte [bx]                       ; 0f b6 17                    ; 0xc1cda
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc1cdd
    test dx, bx                               ; 85 da                       ; 0xc1ce1
    je short 01ce8h                           ; 74 03                       ; 0xc1ce3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ce5 vgabios.c:1233
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1ce8 vgabios.c:1235
    movzx dx, ch                              ; 0f b6 d5                    ; 0xc1ceb
    add dx, word [bp-00ah]                    ; 03 56 f6                    ; 0xc1cee
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1cf1
    call 02f65h                               ; e8 6e 12                    ; 0xc1cf4
    shr byte [bp-008h], 1                     ; d0 6e f8                    ; 0xc1cf7 vgabios.c:1236
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1cfa vgabios.c:1237
    jmp short 01ccah                          ; eb cc                       ; 0xc1cfc
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc1cfe vgabios.c:1238
    cmp cl, 008h                              ; 80 f9 08                    ; 0xc1d00
    jnc short 01d22h                          ; 73 1d                       ; 0xc1d03
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc1d05
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1d08
    imul dx, bx                               ; 0f af d3                    ; 0xc1d0c
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1d0f
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1d12
    add bx, dx                                ; 01 d3                       ; 0xc1d15
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc1d17
    mov byte [bp-008h], 080h                  ; c6 46 f8 80                 ; 0xc1d1a
    xor ch, ch                                ; 30 ed                       ; 0xc1d1e
    jmp short 01ccfh                          ; eb ad                       ; 0xc1d20
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1d22 vgabios.c:1239
    pop di                                    ; 5f                          ; 0xc1d25
    pop si                                    ; 5e                          ; 0xc1d26
    pop bp                                    ; 5d                          ; 0xc1d27
    retn 00002h                               ; c2 02 00                    ; 0xc1d28
  ; disGetNextSymbol 0xc1d2b LB 0x1dae -> off=0x0 cb=0000000000000168 uValue=00000000000c1d2b 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc1d2b LB 0x168
    push bp                                   ; 55                          ; 0xc1d2b vgabios.c:1242
    mov bp, sp                                ; 89 e5                       ; 0xc1d2c
    push si                                   ; 56                          ; 0xc1d2e
    push di                                   ; 57                          ; 0xc1d2f
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1d30
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1d33
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1d36
    mov byte [bp-012h], bl                    ; 88 5e ee                    ; 0xc1d39
    mov si, cx                                ; 89 ce                       ; 0xc1d3c
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1d3e vgabios.c:1249
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1d41
    call 02f57h                               ; e8 10 12                    ; 0xc1d44
    xor ah, ah                                ; 30 e4                       ; 0xc1d47 vgabios.c:1250
    call 02f30h                               ; e8 e4 11                    ; 0xc1d49
    mov cl, al                                ; 88 c1                       ; 0xc1d4c
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1d4e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1d51 vgabios.c:1251
    je near 01e8ch                            ; 0f 84 35 01                 ; 0xc1d53
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1d57 vgabios.c:1254
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc1d5b
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc1d5e
    call 00a88h                               ; e8 24 ed                    ; 0xc1d61
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc1d64 vgabios.c:1255
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1d67
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1d6a
    xor al, al                                ; 30 c0                       ; 0xc1d6d
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1d6f
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1d72
    mov dx, 00084h                            ; ba 84 00                    ; 0xc1d75 vgabios.c:1258
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1d78
    call 02f57h                               ; e8 d9 11                    ; 0xc1d7b
    xor ah, ah                                ; 30 e4                       ; 0xc1d7e
    inc ax                                    ; 40                          ; 0xc1d80
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1d81
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc1d84 vgabios.c:1259
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1d87
    call 02f73h                               ; e8 e6 11                    ; 0xc1d8a
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1d8d
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc1d90 vgabios.c:1261
    mov di, bx                                ; 89 df                       ; 0xc1d93
    sal di, 003h                              ; c1 e7 03                    ; 0xc1d95
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1d98
    jne short 01de6h                          ; 75 47                       ; 0xc1d9d
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc1d9f vgabios.c:1264
    imul bx, ax                               ; 0f af d8                    ; 0xc1da2
    add bx, bx                                ; 01 db                       ; 0xc1da5
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc1da7
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1daa
    inc bx                                    ; 43                          ; 0xc1dae
    imul dx, bx                               ; 0f af d3                    ; 0xc1daf
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1db2
    imul ax, bx                               ; 0f af c3                    ; 0xc1db6
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1db9
    add ax, bx                                ; 01 d8                       ; 0xc1dbd
    add ax, ax                                ; 01 c0                       ; 0xc1dbf
    add dx, ax                                ; 01 c2                       ; 0xc1dc1
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1dc3 vgabios.c:1266
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1dc7
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc1dca
    add ax, bx                                ; 01 d8                       ; 0xc1dce
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1dd0
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1dd3 vgabios.c:1267
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1dd6
    mov cx, si                                ; 89 f1                       ; 0xc1dda
    mov di, dx                                ; 89 d7                       ; 0xc1ddc
    cld                                       ; fc                          ; 0xc1dde
    jcxz 01de3h                               ; e3 02                       ; 0xc1ddf
    rep stosw                                 ; f3 ab                       ; 0xc1de1
    jmp near 01e8ch                           ; e9 a6 00                    ; 0xc1de3 vgabios.c:1269
    movzx bx, byte [bx+046b4h]                ; 0f b6 9f b4 46              ; 0xc1de6 vgabios.c:1272
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1deb
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc1dee
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1df2
    mov al, byte [di+04637h]                  ; 8a 85 37 46                 ; 0xc1df5 vgabios.c:1273
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1df9
    dec si                                    ; 4e                          ; 0xc1dfc vgabios.c:1274
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc1dfd
    je near 01e8ch                            ; 0f 84 88 00                 ; 0xc1e00
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1e04
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e08
    jnc near 01e8ch                           ; 0f 83 7d 00                 ; 0xc1e0b
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1e0f vgabios.c:1276
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1e13
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc1e16
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1e1a
    jc short 01e2ah                           ; 72 0c                       ; 0xc1e1c
    jbe short 01e30h                          ; 76 10                       ; 0xc1e1e
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1e20
    je short 01e6eh                           ; 74 4a                       ; 0xc1e22
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1e24
    je short 01e30h                           ; 74 08                       ; 0xc1e26
    jmp short 01e86h                          ; eb 5c                       ; 0xc1e28
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1e2a
    je short 01e4fh                           ; 74 21                       ; 0xc1e2c
    jmp short 01e86h                          ; eb 56                       ; 0xc1e2e
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1e30 vgabios.c:1280
    push ax                                   ; 50                          ; 0xc1e34
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1e35
    push ax                                   ; 50                          ; 0xc1e39
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e3a
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1e3e
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1e42
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e46
    call 01a91h                               ; e8 44 fc                    ; 0xc1e4a
    jmp short 01e86h                          ; eb 37                       ; 0xc1e4d vgabios.c:1281
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1e4f vgabios.c:1283
    push ax                                   ; 50                          ; 0xc1e53
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1e54
    push ax                                   ; 50                          ; 0xc1e58
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e59
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1e5d
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1e61
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e65
    call 01b7ch                               ; e8 10 fd                    ; 0xc1e69
    jmp short 01e86h                          ; eb 18                       ; 0xc1e6c vgabios.c:1284
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1e6e vgabios.c:1286
    push ax                                   ; 50                          ; 0xc1e72
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e73
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1e77
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1e7b
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e7f
    call 01c9ah                               ; e8 14 fe                    ; 0xc1e83
    inc byte [bp-00ch]                        ; fe 46 f4                    ; 0xc1e86 vgabios.c:1293
    jmp near 01dfch                           ; e9 70 ff                    ; 0xc1e89 vgabios.c:1294
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1e8c vgabios.c:1296
    pop di                                    ; 5f                          ; 0xc1e8f
    pop si                                    ; 5e                          ; 0xc1e90
    pop bp                                    ; 5d                          ; 0xc1e91
    retn                                      ; c3                          ; 0xc1e92
  ; disGetNextSymbol 0xc1e93 LB 0x1c46 -> off=0x0 cb=000000000000016f uValue=00000000000c1e93 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc1e93 LB 0x16f
    push bp                                   ; 55                          ; 0xc1e93 vgabios.c:1299
    mov bp, sp                                ; 89 e5                       ; 0xc1e94
    push si                                   ; 56                          ; 0xc1e96
    push di                                   ; 57                          ; 0xc1e97
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1e98
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1e9b
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc1e9e
    mov byte [bp-014h], bl                    ; 88 5e ec                    ; 0xc1ea1
    mov si, cx                                ; 89 ce                       ; 0xc1ea4
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1ea6 vgabios.c:1306
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1ea9
    call 02f57h                               ; e8 a8 10                    ; 0xc1eac
    xor ah, ah                                ; 30 e4                       ; 0xc1eaf vgabios.c:1307
    call 02f30h                               ; e8 7c 10                    ; 0xc1eb1
    mov cl, al                                ; 88 c1                       ; 0xc1eb4
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1eb6
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1eb9 vgabios.c:1308
    je near 01ffbh                            ; 0f 84 3c 01                 ; 0xc1ebb
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1ebf vgabios.c:1311
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc1ec3
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc1ec6
    call 00a88h                               ; e8 bc eb                    ; 0xc1ec9
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc1ecc vgabios.c:1312
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1ecf
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1ed2
    xor al, al                                ; 30 c0                       ; 0xc1ed5
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1ed7
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1eda
    mov dx, 00084h                            ; ba 84 00                    ; 0xc1edd vgabios.c:1315
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1ee0
    call 02f57h                               ; e8 71 10                    ; 0xc1ee3
    xor ah, ah                                ; 30 e4                       ; 0xc1ee6
    inc ax                                    ; 40                          ; 0xc1ee8
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1ee9
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc1eec vgabios.c:1316
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1eef
    call 02f73h                               ; e8 7e 10                    ; 0xc1ef2
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1ef5
    movzx di, cl                              ; 0f b6 f9                    ; 0xc1ef8 vgabios.c:1318
    mov bx, di                                ; 89 fb                       ; 0xc1efb
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1efd
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1f00
    jne short 01f51h                          ; 75 4a                       ; 0xc1f05
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1f07 vgabios.c:1321
    imul dx, ax                               ; 0f af d0                    ; 0xc1f0a
    add dx, dx                                ; 01 d2                       ; 0xc1f0d
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc1f0f
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1f12
    inc dx                                    ; 42                          ; 0xc1f16
    imul bx, dx                               ; 0f af da                    ; 0xc1f17
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1f1a
    mov cx, ax                                ; 89 c1                       ; 0xc1f1e
    imul cx, dx                               ; 0f af ca                    ; 0xc1f20
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1f23
    add cx, dx                                ; 01 d1                       ; 0xc1f27
    add cx, cx                                ; 01 c9                       ; 0xc1f29
    add cx, bx                                ; 01 d9                       ; 0xc1f2b
    dec si                                    ; 4e                          ; 0xc1f2d vgabios.c:1323
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc1f2e
    je near 01ffbh                            ; 0f 84 c6 00                 ; 0xc1f31
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1f35 vgabios.c:1324
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc1f39
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1f3d
    mov di, word [bx+04638h]                  ; 8b bf 38 46                 ; 0xc1f40
    mov bx, ax                                ; 89 c3                       ; 0xc1f44
    mov dx, cx                                ; 89 ca                       ; 0xc1f46
    mov ax, di                                ; 89 f8                       ; 0xc1f48
    call 02f65h                               ; e8 18 10                    ; 0xc1f4a
    inc cx                                    ; 41                          ; 0xc1f4d vgabios.c:1325
    inc cx                                    ; 41                          ; 0xc1f4e
    jmp short 01f2dh                          ; eb dc                       ; 0xc1f4f vgabios.c:1326
    movzx di, byte [di+046b4h]                ; 0f b6 bd b4 46              ; 0xc1f51 vgabios.c:1331
    sal di, 006h                              ; c1 e7 06                    ; 0xc1f56
    mov al, byte [di+046cah]                  ; 8a 85 ca 46                 ; 0xc1f59
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1f5d
    mov al, byte [bx+04637h]                  ; 8a 87 37 46                 ; 0xc1f60 vgabios.c:1332
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1f64
    dec si                                    ; 4e                          ; 0xc1f67 vgabios.c:1333
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc1f68
    je near 01ffbh                            ; 0f 84 8c 00                 ; 0xc1f6b
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1f6f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f73
    jnc near 01ffbh                           ; 0f 83 81 00                 ; 0xc1f76
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc1f7a vgabios.c:1335
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1f7e
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc1f81
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1f85
    jc short 01f98h                           ; 72 0e                       ; 0xc1f88
    jbe short 01f9fh                          ; 76 13                       ; 0xc1f8a
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc1f8c
    je short 01fddh                           ; 74 4c                       ; 0xc1f8f
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc1f91
    je short 01f9fh                           ; 74 09                       ; 0xc1f94
    jmp short 01ff5h                          ; eb 5d                       ; 0xc1f96
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1f98
    je short 01fbeh                           ; 74 21                       ; 0xc1f9b
    jmp short 01ff5h                          ; eb 56                       ; 0xc1f9d
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc1f9f vgabios.c:1339
    push ax                                   ; 50                          ; 0xc1fa3
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1fa4
    push ax                                   ; 50                          ; 0xc1fa8
    movzx cx, byte [bp-010h]                  ; 0f b6 4e f0                 ; 0xc1fa9
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1fad
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1fb1
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1fb5
    call 01a91h                               ; e8 d5 fa                    ; 0xc1fb9
    jmp short 01ff5h                          ; eb 37                       ; 0xc1fbc vgabios.c:1340
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1fbe vgabios.c:1342
    push ax                                   ; 50                          ; 0xc1fc2
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1fc3
    push ax                                   ; 50                          ; 0xc1fc7
    movzx cx, byte [bp-010h]                  ; 0f b6 4e f0                 ; 0xc1fc8
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1fcc
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1fd0
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1fd4
    call 01b7ch                               ; e8 a1 fb                    ; 0xc1fd8
    jmp short 01ff5h                          ; eb 18                       ; 0xc1fdb vgabios.c:1343
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc1fdd vgabios.c:1345
    push ax                                   ; 50                          ; 0xc1fe1
    movzx cx, byte [bp-010h]                  ; 0f b6 4e f0                 ; 0xc1fe2
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc1fe6
    movzx dx, byte [bp-014h]                  ; 0f b6 56 ec                 ; 0xc1fea
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1fee
    call 01c9ah                               ; e8 a5 fc                    ; 0xc1ff2
    inc byte [bp-00ch]                        ; fe 46 f4                    ; 0xc1ff5 vgabios.c:1352
    jmp near 01f67h                           ; e9 6c ff                    ; 0xc1ff8 vgabios.c:1353
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1ffb vgabios.c:1355
    pop di                                    ; 5f                          ; 0xc1ffe
    pop si                                    ; 5e                          ; 0xc1fff
    pop bp                                    ; 5d                          ; 0xc2000
    retn                                      ; c3                          ; 0xc2001
  ; disGetNextSymbol 0xc2002 LB 0x1ad7 -> off=0x0 cb=000000000000016a uValue=00000000000c2002 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc2002 LB 0x16a
    push bp                                   ; 55                          ; 0xc2002 vgabios.c:1358
    mov bp, sp                                ; 89 e5                       ; 0xc2003
    push si                                   ; 56                          ; 0xc2005
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc2006
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2009
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc200c
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc200f vgabios.c:1364
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2012
    call 02f57h                               ; e8 3f 0f                    ; 0xc2015
    xor ah, ah                                ; 30 e4                       ; 0xc2018 vgabios.c:1365
    call 02f30h                               ; e8 13 0f                    ; 0xc201a
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc201d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2020 vgabios.c:1366
    je near 02144h                            ; 0f 84 1e 01                 ; 0xc2022
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2026 vgabios.c:1367
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2029
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc202c
    je near 02144h                            ; 0f 84 0f 01                 ; 0xc2031
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc2035 vgabios.c:1369
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc2039
    jc short 0204ch                           ; 72 0f                       ; 0xc203b
    jbe short 02053h                          ; 76 14                       ; 0xc203d
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc203f
    je near 0214ah                            ; 0f 84 05 01                 ; 0xc2041
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2045
    je short 02053h                           ; 74 0a                       ; 0xc2047
    jmp near 02144h                           ; e9 f8 00                    ; 0xc2049
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc204c
    je short 020b8h                           ; 74 68                       ; 0xc204e
    jmp near 02144h                           ; e9 f1 00                    ; 0xc2050
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2053 vgabios.c:1373
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2056
    call 02f73h                               ; e8 17 0f                    ; 0xc2059
    imul ax, cx                               ; 0f af c1                    ; 0xc205c
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc205f
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2062
    add bx, ax                                ; 01 c3                       ; 0xc2065
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2067
    mov cx, word [bp-00ah]                    ; 8b 4e f6                    ; 0xc206a vgabios.c:1374
    and cl, 007h                              ; 80 e1 07                    ; 0xc206d
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2070
    sar ax, CL                                ; d3 f8                       ; 0xc2073
    xor ah, ah                                ; 30 e4                       ; 0xc2075 vgabios.c:1375
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2077
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc207a
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc207c
    out DX, ax                                ; ef                          ; 0xc207f
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2080 vgabios.c:1376
    out DX, ax                                ; ef                          ; 0xc2083
    mov dx, bx                                ; 89 da                       ; 0xc2084 vgabios.c:1377
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2086
    call 02f57h                               ; e8 cb 0e                    ; 0xc2089
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc208c vgabios.c:1378
    je short 02099h                           ; 74 07                       ; 0xc2090
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2092 vgabios.c:1380
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2095
    out DX, ax                                ; ef                          ; 0xc2098
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2099 vgabios.c:1382
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc209d
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc20a0
    call 02f65h                               ; e8 bf 0e                    ; 0xc20a3
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc20a6 vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc20a9
    out DX, ax                                ; ef                          ; 0xc20ac
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc20ad vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc20b0
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc20b1 vgabios.c:1385
    out DX, ax                                ; ef                          ; 0xc20b4
    jmp near 02144h                           ; e9 8c 00                    ; 0xc20b5 vgabios.c:1386
    mov ax, cx                                ; 89 c8                       ; 0xc20b8 vgabios.c:1388
    shr ax, 1                                 ; d1 e8                       ; 0xc20ba
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc20bc
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc20bf
    jne short 020ceh                          ; 75 08                       ; 0xc20c4
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc20c6 vgabios.c:1390
    shr bx, 002h                              ; c1 eb 02                    ; 0xc20c9
    jmp short 020d4h                          ; eb 06                       ; 0xc20cc vgabios.c:1392
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc20ce vgabios.c:1394
    shr bx, 003h                              ; c1 eb 03                    ; 0xc20d1
    add bx, ax                                ; 01 c3                       ; 0xc20d4
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc20d6
    test cl, 001h                             ; f6 c1 01                    ; 0xc20d9 vgabios.c:1396
    je short 020e2h                           ; 74 04                       ; 0xc20dc
    add byte [bp-007h], 020h                  ; 80 46 f9 20                 ; 0xc20de
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc20e2 vgabios.c:1397
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc20e5
    call 02f57h                               ; e8 6c 0e                    ; 0xc20e8
    mov bl, al                                ; 88 c3                       ; 0xc20eb
    movzx si, byte [bp-004h]                  ; 0f b6 76 fc                 ; 0xc20ed vgabios.c:1398
    sal si, 003h                              ; c1 e6 03                    ; 0xc20f1
    cmp byte [si+04637h], 002h                ; 80 bc 37 46 02              ; 0xc20f4
    jne short 02114h                          ; 75 19                       ; 0xc20f9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc20fb vgabios.c:1400
    and AL, strict byte 003h                  ; 24 03                       ; 0xc20fe
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc2100
    sub ah, al                                ; 28 c4                       ; 0xc2102
    mov cl, ah                                ; 88 e1                       ; 0xc2104
    add cl, ah                                ; 00 e1                       ; 0xc2106
    mov bh, byte [bp-006h]                    ; 8a 7e fa                    ; 0xc2108
    and bh, 003h                              ; 80 e7 03                    ; 0xc210b
    sal bh, CL                                ; d2 e7                       ; 0xc210e
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2110 vgabios.c:1401
    jmp short 02127h                          ; eb 13                       ; 0xc2112 vgabios.c:1403
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2114 vgabios.c:1405
    and AL, strict byte 007h                  ; 24 07                       ; 0xc2117
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2119
    sub cl, al                                ; 28 c1                       ; 0xc211b
    mov bh, byte [bp-006h]                    ; 8a 7e fa                    ; 0xc211d
    and bh, 001h                              ; 80 e7 01                    ; 0xc2120
    sal bh, CL                                ; d2 e7                       ; 0xc2123
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2125 vgabios.c:1406
    sal al, CL                                ; d2 e0                       ; 0xc2127
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc2129 vgabios.c:1408
    je short 02133h                           ; 74 04                       ; 0xc212d
    xor bl, bh                                ; 30 fb                       ; 0xc212f vgabios.c:1410
    jmp short 02139h                          ; eb 06                       ; 0xc2131 vgabios.c:1412
    not al                                    ; f6 d0                       ; 0xc2133 vgabios.c:1414
    and bl, al                                ; 20 c3                       ; 0xc2135
    or bl, bh                                 ; 08 fb                       ; 0xc2137 vgabios.c:1415
    xor bh, bh                                ; 30 ff                       ; 0xc2139 vgabios.c:1417
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc213b
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc213e
    call 02f65h                               ; e8 21 0e                    ; 0xc2141
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2144 vgabios.c:1418
    pop si                                    ; 5e                          ; 0xc2147
    pop bp                                    ; 5d                          ; 0xc2148
    retn                                      ; c3                          ; 0xc2149
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc214a vgabios.c:1420
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc214d
    call 02f73h                               ; e8 20 0e                    ; 0xc2150
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2153
    imul cx, ax                               ; 0f af c8                    ; 0xc2156
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2159
    add ax, cx                                ; 01 c8                       ; 0xc215c
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc215e
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2161 vgabios.c:1421
    mov dx, ax                                ; 89 c2                       ; 0xc2165
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2167
    jmp short 02141h                          ; eb d5                       ; 0xc216a
  ; disGetNextSymbol 0xc216c LB 0x196d -> off=0x0 cb=0000000000000241 uValue=00000000000c216c 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc216c LB 0x241
    push bp                                   ; 55                          ; 0xc216c vgabios.c:1431
    mov bp, sp                                ; 89 e5                       ; 0xc216d
    push si                                   ; 56                          ; 0xc216f
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2170
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2173
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2176
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc2179
    mov byte [bp-00eh], cl                    ; 88 4e f2                    ; 0xc217c
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc217f vgabios.c:1439
    jne short 02190h                          ; 75 0c                       ; 0xc2182
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2184 vgabios.c:1440
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2187
    call 02f57h                               ; e8 ca 0d                    ; 0xc218a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc218d
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2190 vgabios.c:1443
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2193
    call 02f57h                               ; e8 be 0d                    ; 0xc2196
    xor ah, ah                                ; 30 e4                       ; 0xc2199 vgabios.c:1444
    call 02f30h                               ; e8 92 0d                    ; 0xc219b
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc219e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc21a1 vgabios.c:1445
    je near 023a7h                            ; 0f 84 00 02                 ; 0xc21a3
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc21a7 vgabios.c:1448
    lea bx, [bp-018h]                         ; 8d 5e e8                    ; 0xc21ab
    lea dx, [bp-016h]                         ; 8d 56 ea                    ; 0xc21ae
    call 00a88h                               ; e8 d4 e8                    ; 0xc21b1
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc21b4 vgabios.c:1449
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc21b7
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc21ba
    xor al, al                                ; 30 c0                       ; 0xc21bd
    shr ax, 008h                              ; c1 e8 08                    ; 0xc21bf
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc21c2
    mov dx, 00084h                            ; ba 84 00                    ; 0xc21c5 vgabios.c:1452
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc21c8
    call 02f57h                               ; e8 89 0d                    ; 0xc21cb
    xor ah, ah                                ; 30 e4                       ; 0xc21ce
    inc ax                                    ; 40                          ; 0xc21d0
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc21d1
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc21d4 vgabios.c:1453
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc21d7
    call 02f73h                               ; e8 96 0d                    ; 0xc21da
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc21dd
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc21e0 vgabios.c:1455
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc21e3
    jc short 021f5h                           ; 72 0e                       ; 0xc21e5
    jbe short 021fdh                          ; 76 14                       ; 0xc21e7
    cmp AL, strict byte 00dh                  ; 3c 0d                       ; 0xc21e9
    je short 0220bh                           ; 74 1e                       ; 0xc21eb
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc21ed
    je near 022fdh                            ; 0f 84 0a 01                 ; 0xc21ef
    jmp short 02212h                          ; eb 1d                       ; 0xc21f3
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc21f5
    je near 02300h                            ; 0f 84 05 01                 ; 0xc21f7
    jmp short 02212h                          ; eb 15                       ; 0xc21fb
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc21fd vgabios.c:1462
    jbe near 02300h                           ; 0f 86 fb 00                 ; 0xc2201
    dec byte [bp-008h]                        ; fe 4e f8                    ; 0xc2205
    jmp near 02300h                           ; e9 f5 00                    ; 0xc2208 vgabios.c:1463
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc220b vgabios.c:1470
    jmp near 02300h                           ; e9 ee 00                    ; 0xc220f vgabios.c:1471
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2212 vgabios.c:1475
    mov si, bx                                ; 89 de                       ; 0xc2216
    sal si, 003h                              ; c1 e6 03                    ; 0xc2218
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc221b
    jne short 0226fh                          ; 75 4d                       ; 0xc2220
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2222 vgabios.c:1478
    imul ax, word [bp-014h]                   ; 0f af 46 ec                 ; 0xc2225
    add ax, ax                                ; 01 c0                       ; 0xc2229
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc222b
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc222d
    mov cx, ax                                ; 89 c1                       ; 0xc2231
    inc cx                                    ; 41                          ; 0xc2233
    imul cx, dx                               ; 0f af ca                    ; 0xc2234
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2237
    imul ax, word [bp-012h]                   ; 0f af 46 ee                 ; 0xc223b
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc223f
    add ax, dx                                ; 01 d0                       ; 0xc2243
    add ax, ax                                ; 01 c0                       ; 0xc2245
    add cx, ax                                ; 01 c1                       ; 0xc2247
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc2249 vgabios.c:1481
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc224d
    mov dx, cx                                ; 89 ca                       ; 0xc2251
    call 02f65h                               ; e8 0f 0d                    ; 0xc2253
    cmp byte [bp-00eh], 003h                  ; 80 7e f2 03                 ; 0xc2256 vgabios.c:1483
    jne near 022edh                           ; 0f 85 8f 00                 ; 0xc225a
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc                 ; 0xc225e vgabios.c:1484
    mov dx, cx                                ; 89 ca                       ; 0xc2262
    inc dx                                    ; 42                          ; 0xc2264
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc2265
    call 02f65h                               ; e8 f9 0c                    ; 0xc2269
    jmp near 022edh                           ; e9 7e 00                    ; 0xc226c vgabios.c:1486
    movzx bx, byte [bx+046b4h]                ; 0f b6 9f b4 46              ; 0xc226f vgabios.c:1489
    sal bx, 006h                              ; c1 e3 06                    ; 0xc2274
    mov ah, byte [bx+046cah]                  ; 8a a7 ca 46                 ; 0xc2277
    mov dl, byte [si+04637h]                  ; 8a 94 37 46                 ; 0xc227b vgabios.c:1490
    mov al, byte [si+04636h]                  ; 8a 84 36 46                 ; 0xc227f vgabios.c:1491
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc2283
    jc short 02293h                           ; 72 0c                       ; 0xc2285
    jbe short 02299h                          ; 76 10                       ; 0xc2287
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2289
    je short 022d5h                           ; 74 48                       ; 0xc228b
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc228d
    je short 02299h                           ; 74 08                       ; 0xc228f
    jmp short 022edh                          ; eb 5a                       ; 0xc2291
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2293
    je short 022b7h                           ; 74 20                       ; 0xc2295
    jmp short 022edh                          ; eb 54                       ; 0xc2297
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc2299 vgabios.c:1495
    push ax                                   ; 50                          ; 0xc229c
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc229d
    push ax                                   ; 50                          ; 0xc22a1
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc22a2
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc22a6
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc22aa
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc22ae
    call 01a91h                               ; e8 dc f7                    ; 0xc22b2
    jmp short 022edh                          ; eb 36                       ; 0xc22b5 vgabios.c:1496
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc22b7 vgabios.c:1498
    push ax                                   ; 50                          ; 0xc22ba
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc22bb
    push ax                                   ; 50                          ; 0xc22bf
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc22c0
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc22c4
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc22c8
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc22cc
    call 01b7ch                               ; e8 a9 f8                    ; 0xc22d0
    jmp short 022edh                          ; eb 18                       ; 0xc22d3 vgabios.c:1499
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc22d5 vgabios.c:1501
    push ax                                   ; 50                          ; 0xc22d9
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc22da
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc22de
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc22e2
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc22e6
    call 01c9ah                               ; e8 ad f9                    ; 0xc22ea
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc22ed vgabios.c:1509
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc22f0 vgabios.c:1511
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc22f4
    jne short 02300h                          ; 75 07                       ; 0xc22f7
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc22f9 vgabios.c:1512
    inc byte [bp-00ah]                        ; fe 46 f6                    ; 0xc22fd vgabios.c:1513
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2300 vgabios.c:1518
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc2304
    jne near 0238bh                           ; 0f 85 80 00                 ; 0xc2307
    movzx si, byte [bp-010h]                  ; 0f b6 76 f0                 ; 0xc230b vgabios.c:1520
    sal si, 003h                              ; c1 e6 03                    ; 0xc230f
    mov bh, byte [bp-014h]                    ; 8a 7e ec                    ; 0xc2312
    db  0feh, 0cfh
    ; dec bh                                    ; fe cf                     ; 0xc2315
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc2317
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc231a
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc231c
    jne short 0236dh                          ; 75 4a                       ; 0xc2321
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2323 vgabios.c:1522
    imul ax, word [bp-014h]                   ; 0f af 46 ec                 ; 0xc2326
    add ax, ax                                ; 01 c0                       ; 0xc232a
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc232c
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc232e
    mov cx, ax                                ; 89 c1                       ; 0xc2332
    inc cx                                    ; 41                          ; 0xc2334
    imul cx, dx                               ; 0f af ca                    ; 0xc2335
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2338
    dec ax                                    ; 48                          ; 0xc233c
    imul ax, word [bp-012h]                   ; 0f af 46 ee                 ; 0xc233d
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc2341
    add ax, dx                                ; 01 d0                       ; 0xc2345
    add ax, ax                                ; 01 c0                       ; 0xc2347
    mov dx, cx                                ; 89 ca                       ; 0xc2349
    add dx, ax                                ; 01 c2                       ; 0xc234b
    inc dx                                    ; 42                          ; 0xc234d
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc234e
    call 02f57h                               ; e8 02 0c                    ; 0xc2352
    push strict byte 00001h                   ; 6a 01                       ; 0xc2355 vgabios.c:1524
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2357
    push dx                                   ; 52                          ; 0xc235b
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc235c
    push dx                                   ; 52                          ; 0xc235f
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc2360
    push dx                                   ; 52                          ; 0xc2363
    movzx dx, al                              ; 0f b6 d0                    ; 0xc2364
    xor cx, cx                                ; 31 c9                       ; 0xc2367
    xor bx, bx                                ; 31 db                       ; 0xc2369
    jmp short 02382h                          ; eb 15                       ; 0xc236b vgabios.c:1526
    push strict byte 00001h                   ; 6a 01                       ; 0xc236d vgabios.c:1528
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc236f
    push ax                                   ; 50                          ; 0xc2373
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc2374
    push ax                                   ; 50                          ; 0xc2377
    movzx ax, bh                              ; 0f b6 c7                    ; 0xc2378
    push ax                                   ; 50                          ; 0xc237b
    xor cx, cx                                ; 31 c9                       ; 0xc237c
    xor bx, bx                                ; 31 db                       ; 0xc237e
    xor dx, dx                                ; 31 d2                       ; 0xc2380
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2382
    call 0158bh                               ; e8 03 f2                    ; 0xc2385
    dec byte [bp-00ah]                        ; fe 4e f6                    ; 0xc2388 vgabios.c:1530
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc238b vgabios.c:1534
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc238f
    sal word [bp-018h], 008h                  ; c1 66 e8 08                 ; 0xc2392
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2396
    add word [bp-018h], ax                    ; 01 46 e8                    ; 0xc239a
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc239d vgabios.c:1535
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc23a0
    call 00e5eh                               ; e8 b7 ea                    ; 0xc23a4
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc23a7 vgabios.c:1536
    pop si                                    ; 5e                          ; 0xc23aa
    pop bp                                    ; 5d                          ; 0xc23ab
    retn                                      ; c3                          ; 0xc23ac
  ; disGetNextSymbol 0xc23ad LB 0x172c -> off=0x0 cb=000000000000002c uValue=00000000000c23ad 'get_font_access'
get_font_access:                             ; 0xc23ad LB 0x2c
    push bp                                   ; 55                          ; 0xc23ad vgabios.c:1539
    mov bp, sp                                ; 89 e5                       ; 0xc23ae
    push dx                                   ; 52                          ; 0xc23b0
    mov ax, 00100h                            ; b8 00 01                    ; 0xc23b1 vgabios.c:1541
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc23b4
    out DX, ax                                ; ef                          ; 0xc23b7
    mov ax, 00402h                            ; b8 02 04                    ; 0xc23b8 vgabios.c:1542
    out DX, ax                                ; ef                          ; 0xc23bb
    mov ax, 00704h                            ; b8 04 07                    ; 0xc23bc vgabios.c:1543
    out DX, ax                                ; ef                          ; 0xc23bf
    mov ax, 00300h                            ; b8 00 03                    ; 0xc23c0 vgabios.c:1544
    out DX, ax                                ; ef                          ; 0xc23c3
    mov ax, 00204h                            ; b8 04 02                    ; 0xc23c4 vgabios.c:1545
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc23c7
    out DX, ax                                ; ef                          ; 0xc23ca
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc23cb vgabios.c:1546
    out DX, ax                                ; ef                          ; 0xc23ce
    mov ax, 00406h                            ; b8 06 04                    ; 0xc23cf vgabios.c:1547
    out DX, ax                                ; ef                          ; 0xc23d2
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc23d3 vgabios.c:1548
    pop dx                                    ; 5a                          ; 0xc23d6
    pop bp                                    ; 5d                          ; 0xc23d7
    retn                                      ; c3                          ; 0xc23d8
  ; disGetNextSymbol 0xc23d9 LB 0x1700 -> off=0x0 cb=000000000000003c uValue=00000000000c23d9 'release_font_access'
release_font_access:                         ; 0xc23d9 LB 0x3c
    push bp                                   ; 55                          ; 0xc23d9 vgabios.c:1550
    mov bp, sp                                ; 89 e5                       ; 0xc23da
    push dx                                   ; 52                          ; 0xc23dc
    mov ax, 00100h                            ; b8 00 01                    ; 0xc23dd vgabios.c:1552
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc23e0
    out DX, ax                                ; ef                          ; 0xc23e3
    mov ax, 00302h                            ; b8 02 03                    ; 0xc23e4 vgabios.c:1553
    out DX, ax                                ; ef                          ; 0xc23e7
    mov ax, 00304h                            ; b8 04 03                    ; 0xc23e8 vgabios.c:1554
    out DX, ax                                ; ef                          ; 0xc23eb
    mov ax, 00300h                            ; b8 00 03                    ; 0xc23ec vgabios.c:1555
    out DX, ax                                ; ef                          ; 0xc23ef
    mov dx, 003cch                            ; ba cc 03                    ; 0xc23f0 vgabios.c:1556
    in AL, DX                                 ; ec                          ; 0xc23f3
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc23f4
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc23f6
    sal ax, 002h                              ; c1 e0 02                    ; 0xc23f9
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc23fc
    sal ax, 008h                              ; c1 e0 08                    ; 0xc23fe
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2401
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2403
    out DX, ax                                ; ef                          ; 0xc2406
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2407 vgabios.c:1557
    out DX, ax                                ; ef                          ; 0xc240a
    mov ax, 01005h                            ; b8 05 10                    ; 0xc240b vgabios.c:1558
    out DX, ax                                ; ef                          ; 0xc240e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc240f vgabios.c:1559
    pop dx                                    ; 5a                          ; 0xc2412
    pop bp                                    ; 5d                          ; 0xc2413
    retn                                      ; c3                          ; 0xc2414
  ; disGetNextSymbol 0xc2415 LB 0x16c4 -> off=0x0 cb=00000000000000bf uValue=00000000000c2415 'set_scan_lines'
set_scan_lines:                              ; 0xc2415 LB 0xbf
    push bp                                   ; 55                          ; 0xc2415 vgabios.c:1561
    mov bp, sp                                ; 89 e5                       ; 0xc2416
    push bx                                   ; 53                          ; 0xc2418
    push cx                                   ; 51                          ; 0xc2419
    push dx                                   ; 52                          ; 0xc241a
    push si                                   ; 56                          ; 0xc241b
    push di                                   ; 57                          ; 0xc241c
    mov bl, al                                ; 88 c3                       ; 0xc241d
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc241f vgabios.c:1566
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2422
    call 02f73h                               ; e8 4b 0b                    ; 0xc2425
    mov dx, ax                                ; 89 c2                       ; 0xc2428
    mov si, ax                                ; 89 c6                       ; 0xc242a
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc242c vgabios.c:1567
    out DX, AL                                ; ee                          ; 0xc242e
    inc dx                                    ; 42                          ; 0xc242f vgabios.c:1568
    in AL, DX                                 ; ec                          ; 0xc2430
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2431
    mov ah, al                                ; 88 c4                       ; 0xc2433 vgabios.c:1569
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2435
    mov al, bl                                ; 88 d8                       ; 0xc2438
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc243a
    or al, ah                                 ; 08 e0                       ; 0xc243c
    out DX, AL                                ; ee                          ; 0xc243e vgabios.c:1570
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc243f vgabios.c:1571
    jne short 0244ch                          ; 75 08                       ; 0xc2442
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2444 vgabios.c:1573
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2447
    jmp short 02459h                          ; eb 0d                       ; 0xc244a vgabios.c:1575
    mov al, bl                                ; 88 d8                       ; 0xc244c vgabios.c:1577
    sub AL, strict byte 003h                  ; 2c 03                       ; 0xc244e
    movzx dx, al                              ; 0f b6 d0                    ; 0xc2450
    mov al, bl                                ; 88 d8                       ; 0xc2453
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2455
    xor ah, ah                                ; 30 e4                       ; 0xc2457
    call 00dbah                               ; e8 5e e9                    ; 0xc2459
    movzx di, bl                              ; 0f b6 fb                    ; 0xc245c vgabios.c:1579
    mov bx, di                                ; 89 fb                       ; 0xc245f
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2461
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2464
    call 02f81h                               ; e8 17 0b                    ; 0xc2467
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc246a vgabios.c:1580
    mov dx, si                                ; 89 f2                       ; 0xc246c
    out DX, AL                                ; ee                          ; 0xc246e
    lea cx, [si+001h]                         ; 8d 4c 01                    ; 0xc246f vgabios.c:1581
    mov dx, cx                                ; 89 ca                       ; 0xc2472
    in AL, DX                                 ; ec                          ; 0xc2474
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2475
    mov bx, ax                                ; 89 c3                       ; 0xc2477
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2479 vgabios.c:1582
    mov dx, si                                ; 89 f2                       ; 0xc247b
    out DX, AL                                ; ee                          ; 0xc247d
    mov dx, cx                                ; 89 ca                       ; 0xc247e vgabios.c:1583
    in AL, DX                                 ; ec                          ; 0xc2480
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2481
    mov ah, al                                ; 88 c4                       ; 0xc2483 vgabios.c:1584
    and ah, 002h                              ; 80 e4 02                    ; 0xc2485
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc2488
    sal dx, 007h                              ; c1 e2 07                    ; 0xc248b
    and AL, strict byte 040h                  ; 24 40                       ; 0xc248e
    xor ah, ah                                ; 30 e4                       ; 0xc2490
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2492
    add ax, dx                                ; 01 d0                       ; 0xc2495
    inc ax                                    ; 40                          ; 0xc2497
    add ax, bx                                ; 01 d8                       ; 0xc2498
    xor dx, dx                                ; 31 d2                       ; 0xc249a vgabios.c:1585
    div di                                    ; f7 f7                       ; 0xc249c
    mov cx, ax                                ; 89 c1                       ; 0xc249e
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc24a0 vgabios.c:1586
    movzx bx, al                              ; 0f b6 d8                    ; 0xc24a2
    mov dx, 00084h                            ; ba 84 00                    ; 0xc24a5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24a8
    call 02f65h                               ; e8 b7 0a                    ; 0xc24ab
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc24ae vgabios.c:1587
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24b1
    call 02f73h                               ; e8 bc 0a                    ; 0xc24b4
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc24b7 vgabios.c:1588
    mov bx, ax                                ; 89 c3                       ; 0xc24ba
    imul bx, dx                               ; 0f af da                    ; 0xc24bc
    add bx, bx                                ; 01 db                       ; 0xc24bf
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc24c1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24c4
    call 02f81h                               ; e8 b7 0a                    ; 0xc24c7
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc24ca vgabios.c:1589
    pop di                                    ; 5f                          ; 0xc24cd
    pop si                                    ; 5e                          ; 0xc24ce
    pop dx                                    ; 5a                          ; 0xc24cf
    pop cx                                    ; 59                          ; 0xc24d0
    pop bx                                    ; 5b                          ; 0xc24d1
    pop bp                                    ; 5d                          ; 0xc24d2
    retn                                      ; c3                          ; 0xc24d3
  ; disGetNextSymbol 0xc24d4 LB 0x1605 -> off=0x0 cb=000000000000007d uValue=00000000000c24d4 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc24d4 LB 0x7d
    push bp                                   ; 55                          ; 0xc24d4 vgabios.c:1591
    mov bp, sp                                ; 89 e5                       ; 0xc24d5
    push si                                   ; 56                          ; 0xc24d7
    push di                                   ; 57                          ; 0xc24d8
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc24d9
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc24dc
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc24df
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc24e2
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc24e5
    call 023adh                               ; e8 c2 fe                    ; 0xc24e8 vgabios.c:1596
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc24eb vgabios.c:1597
    and AL, strict byte 003h                  ; 24 03                       ; 0xc24ee
    xor ah, ah                                ; 30 e4                       ; 0xc24f0
    mov bx, ax                                ; 89 c3                       ; 0xc24f2
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc24f4
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc24f7
    and AL, strict byte 004h                  ; 24 04                       ; 0xc24fa
    xor ah, ah                                ; 30 e4                       ; 0xc24fc
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc24fe
    add bx, ax                                ; 01 c3                       ; 0xc2501
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc2503
    xor bx, bx                                ; 31 db                       ; 0xc2506 vgabios.c:1598
    cmp bx, word [bp-00ah]                    ; 3b 5e f6                    ; 0xc2508
    jnc short 02538h                          ; 73 2b                       ; 0xc250b
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc250d vgabios.c:1600
    mov si, bx                                ; 89 de                       ; 0xc2511
    imul si, cx                               ; 0f af f1                    ; 0xc2513
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc2516
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2519 vgabios.c:1601
    add di, bx                                ; 01 df                       ; 0xc251c
    sal di, 005h                              ; c1 e7 05                    ; 0xc251e
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc2521
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc2524 vgabios.c:1602
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2527
    mov es, ax                                ; 8e c0                       ; 0xc252a
    cld                                       ; fc                          ; 0xc252c
    jcxz 02535h                               ; e3 06                       ; 0xc252d
    push DS                                   ; 1e                          ; 0xc252f
    mov ds, dx                                ; 8e da                       ; 0xc2530
    rep movsb                                 ; f3 a4                       ; 0xc2532
    pop DS                                    ; 1f                          ; 0xc2534
    inc bx                                    ; 43                          ; 0xc2535 vgabios.c:1603
    jmp short 02508h                          ; eb d0                       ; 0xc2536
    call 023d9h                               ; e8 9e fe                    ; 0xc2538 vgabios.c:1604
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc253b vgabios.c:1605
    jc short 02548h                           ; 72 07                       ; 0xc253f
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08                 ; 0xc2541 vgabios.c:1607
    call 02415h                               ; e8 cd fe                    ; 0xc2545
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2548 vgabios.c:1609
    pop di                                    ; 5f                          ; 0xc254b
    pop si                                    ; 5e                          ; 0xc254c
    pop bp                                    ; 5d                          ; 0xc254d
    retn 00006h                               ; c2 06 00                    ; 0xc254e
  ; disGetNextSymbol 0xc2551 LB 0x1588 -> off=0x0 cb=0000000000000070 uValue=00000000000c2551 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2551 LB 0x70
    push bp                                   ; 55                          ; 0xc2551 vgabios.c:1611
    mov bp, sp                                ; 89 e5                       ; 0xc2552
    push bx                                   ; 53                          ; 0xc2554
    push cx                                   ; 51                          ; 0xc2555
    push si                                   ; 56                          ; 0xc2556
    push di                                   ; 57                          ; 0xc2557
    push ax                                   ; 50                          ; 0xc2558
    push ax                                   ; 50                          ; 0xc2559
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc255a
    call 023adh                               ; e8 4d fe                    ; 0xc255d vgabios.c:1615
    mov al, dl                                ; 88 d0                       ; 0xc2560 vgabios.c:1616
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2562
    xor ah, ah                                ; 30 e4                       ; 0xc2564
    mov bx, ax                                ; 89 c3                       ; 0xc2566
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2568
    mov al, dl                                ; 88 d0                       ; 0xc256b
    and AL, strict byte 004h                  ; 24 04                       ; 0xc256d
    xor ah, ah                                ; 30 e4                       ; 0xc256f
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2571
    add bx, ax                                ; 01 c3                       ; 0xc2574
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2576
    xor bx, bx                                ; 31 db                       ; 0xc2579 vgabios.c:1617
    jmp short 02583h                          ; eb 06                       ; 0xc257b
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc257d
    jnc short 025a9h                          ; 73 26                       ; 0xc2581
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2583 vgabios.c:1619
    mov di, bx                                ; 89 df                       ; 0xc2586 vgabios.c:1620
    sal di, 005h                              ; c1 e7 05                    ; 0xc2588
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc258b
    add si, 05bf2h                            ; 81 c6 f2 5b                 ; 0xc258e vgabios.c:1621
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2592
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2595
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2598
    mov es, ax                                ; 8e c0                       ; 0xc259b
    cld                                       ; fc                          ; 0xc259d
    jcxz 025a6h                               ; e3 06                       ; 0xc259e
    push DS                                   ; 1e                          ; 0xc25a0
    mov ds, dx                                ; 8e da                       ; 0xc25a1
    rep movsb                                 ; f3 a4                       ; 0xc25a3
    pop DS                                    ; 1f                          ; 0xc25a5
    inc bx                                    ; 43                          ; 0xc25a6 vgabios.c:1622
    jmp short 0257dh                          ; eb d4                       ; 0xc25a7
    call 023d9h                               ; e8 2d fe                    ; 0xc25a9 vgabios.c:1623
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc25ac vgabios.c:1624
    jc short 025b8h                           ; 72 06                       ; 0xc25b0
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc25b2 vgabios.c:1626
    call 02415h                               ; e8 5d fe                    ; 0xc25b5
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc25b8 vgabios.c:1628
    pop di                                    ; 5f                          ; 0xc25bb
    pop si                                    ; 5e                          ; 0xc25bc
    pop cx                                    ; 59                          ; 0xc25bd
    pop bx                                    ; 5b                          ; 0xc25be
    pop bp                                    ; 5d                          ; 0xc25bf
    retn                                      ; c3                          ; 0xc25c0
  ; disGetNextSymbol 0xc25c1 LB 0x1518 -> off=0x0 cb=0000000000000072 uValue=00000000000c25c1 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc25c1 LB 0x72
    push bp                                   ; 55                          ; 0xc25c1 vgabios.c:1630
    mov bp, sp                                ; 89 e5                       ; 0xc25c2
    push bx                                   ; 53                          ; 0xc25c4
    push cx                                   ; 51                          ; 0xc25c5
    push si                                   ; 56                          ; 0xc25c6
    push di                                   ; 57                          ; 0xc25c7
    push ax                                   ; 50                          ; 0xc25c8
    push ax                                   ; 50                          ; 0xc25c9
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc25ca
    call 023adh                               ; e8 dd fd                    ; 0xc25cd vgabios.c:1634
    mov al, dl                                ; 88 d0                       ; 0xc25d0 vgabios.c:1635
    and AL, strict byte 003h                  ; 24 03                       ; 0xc25d2
    xor ah, ah                                ; 30 e4                       ; 0xc25d4
    mov bx, ax                                ; 89 c3                       ; 0xc25d6
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc25d8
    mov al, dl                                ; 88 d0                       ; 0xc25db
    and AL, strict byte 004h                  ; 24 04                       ; 0xc25dd
    xor ah, ah                                ; 30 e4                       ; 0xc25df
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc25e1
    add bx, ax                                ; 01 c3                       ; 0xc25e4
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc25e6
    xor bx, bx                                ; 31 db                       ; 0xc25e9 vgabios.c:1636
    jmp short 025f3h                          ; eb 06                       ; 0xc25eb
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc25ed
    jnc short 0261bh                          ; 73 28                       ; 0xc25f1
    mov si, bx                                ; 89 de                       ; 0xc25f3 vgabios.c:1638
    sal si, 003h                              ; c1 e6 03                    ; 0xc25f5
    mov di, bx                                ; 89 df                       ; 0xc25f8 vgabios.c:1639
    sal di, 005h                              ; c1 e7 05                    ; 0xc25fa
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc25fd
    add si, 053f2h                            ; 81 c6 f2 53                 ; 0xc2600 vgabios.c:1640
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2604
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2607
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc260a
    mov es, ax                                ; 8e c0                       ; 0xc260d
    cld                                       ; fc                          ; 0xc260f
    jcxz 02618h                               ; e3 06                       ; 0xc2610
    push DS                                   ; 1e                          ; 0xc2612
    mov ds, dx                                ; 8e da                       ; 0xc2613
    rep movsb                                 ; f3 a4                       ; 0xc2615
    pop DS                                    ; 1f                          ; 0xc2617
    inc bx                                    ; 43                          ; 0xc2618 vgabios.c:1641
    jmp short 025edh                          ; eb d2                       ; 0xc2619
    call 023d9h                               ; e8 bb fd                    ; 0xc261b vgabios.c:1642
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc261e vgabios.c:1643
    jc short 0262ah                           ; 72 06                       ; 0xc2622
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2624 vgabios.c:1645
    call 02415h                               ; e8 eb fd                    ; 0xc2627
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc262a vgabios.c:1647
    pop di                                    ; 5f                          ; 0xc262d
    pop si                                    ; 5e                          ; 0xc262e
    pop cx                                    ; 59                          ; 0xc262f
    pop bx                                    ; 5b                          ; 0xc2630
    pop bp                                    ; 5d                          ; 0xc2631
    retn                                      ; c3                          ; 0xc2632
  ; disGetNextSymbol 0xc2633 LB 0x14a6 -> off=0x0 cb=0000000000000072 uValue=00000000000c2633 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2633 LB 0x72
    push bp                                   ; 55                          ; 0xc2633 vgabios.c:1650
    mov bp, sp                                ; 89 e5                       ; 0xc2634
    push bx                                   ; 53                          ; 0xc2636
    push cx                                   ; 51                          ; 0xc2637
    push si                                   ; 56                          ; 0xc2638
    push di                                   ; 57                          ; 0xc2639
    push ax                                   ; 50                          ; 0xc263a
    push ax                                   ; 50                          ; 0xc263b
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc263c
    call 023adh                               ; e8 6b fd                    ; 0xc263f vgabios.c:1654
    mov al, dl                                ; 88 d0                       ; 0xc2642 vgabios.c:1655
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2644
    xor ah, ah                                ; 30 e4                       ; 0xc2646
    mov bx, ax                                ; 89 c3                       ; 0xc2648
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc264a
    mov al, dl                                ; 88 d0                       ; 0xc264d
    and AL, strict byte 004h                  ; 24 04                       ; 0xc264f
    xor ah, ah                                ; 30 e4                       ; 0xc2651
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2653
    add bx, ax                                ; 01 c3                       ; 0xc2656
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2658
    xor bx, bx                                ; 31 db                       ; 0xc265b vgabios.c:1656
    jmp short 02665h                          ; eb 06                       ; 0xc265d
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc265f
    jnc short 0268dh                          ; 73 28                       ; 0xc2663
    mov si, bx                                ; 89 de                       ; 0xc2665 vgabios.c:1658
    sal si, 004h                              ; c1 e6 04                    ; 0xc2667
    mov di, bx                                ; 89 df                       ; 0xc266a vgabios.c:1659
    sal di, 005h                              ; c1 e7 05                    ; 0xc266c
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc266f
    add si, 069f2h                            ; 81 c6 f2 69                 ; 0xc2672 vgabios.c:1660
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2676
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2679
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc267c
    mov es, ax                                ; 8e c0                       ; 0xc267f
    cld                                       ; fc                          ; 0xc2681
    jcxz 0268ah                               ; e3 06                       ; 0xc2682
    push DS                                   ; 1e                          ; 0xc2684
    mov ds, dx                                ; 8e da                       ; 0xc2685
    rep movsb                                 ; f3 a4                       ; 0xc2687
    pop DS                                    ; 1f                          ; 0xc2689
    inc bx                                    ; 43                          ; 0xc268a vgabios.c:1661
    jmp short 0265fh                          ; eb d2                       ; 0xc268b
    call 023d9h                               ; e8 49 fd                    ; 0xc268d vgabios.c:1662
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2690 vgabios.c:1663
    jc short 0269ch                           ; 72 06                       ; 0xc2694
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2696 vgabios.c:1665
    call 02415h                               ; e8 79 fd                    ; 0xc2699
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc269c vgabios.c:1667
    pop di                                    ; 5f                          ; 0xc269f
    pop si                                    ; 5e                          ; 0xc26a0
    pop cx                                    ; 59                          ; 0xc26a1
    pop bx                                    ; 5b                          ; 0xc26a2
    pop bp                                    ; 5d                          ; 0xc26a3
    retn                                      ; c3                          ; 0xc26a4
  ; disGetNextSymbol 0xc26a5 LB 0x1434 -> off=0x0 cb=0000000000000005 uValue=00000000000c26a5 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc26a5 LB 0x5
    push bp                                   ; 55                          ; 0xc26a5 vgabios.c:1669
    mov bp, sp                                ; 89 e5                       ; 0xc26a6
    pop bp                                    ; 5d                          ; 0xc26a8 vgabios.c:1674
    retn                                      ; c3                          ; 0xc26a9
  ; disGetNextSymbol 0xc26aa LB 0x142f -> off=0x0 cb=0000000000000007 uValue=00000000000c26aa 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc26aa LB 0x7
    push bp                                   ; 55                          ; 0xc26aa vgabios.c:1675
    mov bp, sp                                ; 89 e5                       ; 0xc26ab
    pop bp                                    ; 5d                          ; 0xc26ad vgabios.c:1681
    retn 00002h                               ; c2 02 00                    ; 0xc26ae
  ; disGetNextSymbol 0xc26b1 LB 0x1428 -> off=0x0 cb=0000000000000005 uValue=00000000000c26b1 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc26b1 LB 0x5
    push bp                                   ; 55                          ; 0xc26b1 vgabios.c:1682
    mov bp, sp                                ; 89 e5                       ; 0xc26b2
    pop bp                                    ; 5d                          ; 0xc26b4 vgabios.c:1687
    retn                                      ; c3                          ; 0xc26b5
  ; disGetNextSymbol 0xc26b6 LB 0x1423 -> off=0x0 cb=0000000000000005 uValue=00000000000c26b6 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc26b6 LB 0x5
    push bp                                   ; 55                          ; 0xc26b6 vgabios.c:1688
    mov bp, sp                                ; 89 e5                       ; 0xc26b7
    pop bp                                    ; 5d                          ; 0xc26b9 vgabios.c:1693
    retn                                      ; c3                          ; 0xc26ba
  ; disGetNextSymbol 0xc26bb LB 0x141e -> off=0x0 cb=0000000000000005 uValue=00000000000c26bb 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc26bb LB 0x5
    push bp                                   ; 55                          ; 0xc26bb vgabios.c:1694
    mov bp, sp                                ; 89 e5                       ; 0xc26bc
    pop bp                                    ; 5d                          ; 0xc26be vgabios.c:1699
    retn                                      ; c3                          ; 0xc26bf
  ; disGetNextSymbol 0xc26c0 LB 0x1419 -> off=0x0 cb=0000000000000005 uValue=00000000000c26c0 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc26c0 LB 0x5
    push bp                                   ; 55                          ; 0xc26c0 vgabios.c:1701
    mov bp, sp                                ; 89 e5                       ; 0xc26c1
    pop bp                                    ; 5d                          ; 0xc26c3 vgabios.c:1706
    retn                                      ; c3                          ; 0xc26c4
  ; disGetNextSymbol 0xc26c5 LB 0x1414 -> off=0x0 cb=0000000000000005 uValue=00000000000c26c5 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc26c5 LB 0x5
    push bp                                   ; 55                          ; 0xc26c5 vgabios.c:1709
    mov bp, sp                                ; 89 e5                       ; 0xc26c6
    pop bp                                    ; 5d                          ; 0xc26c8 vgabios.c:1714
    retn                                      ; c3                          ; 0xc26c9
  ; disGetNextSymbol 0xc26ca LB 0x140f -> off=0x0 cb=0000000000000005 uValue=00000000000c26ca 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc26ca LB 0x5
    push bp                                   ; 55                          ; 0xc26ca vgabios.c:1715
    mov bp, sp                                ; 89 e5                       ; 0xc26cb
    pop bp                                    ; 5d                          ; 0xc26cd vgabios.c:1720
    retn                                      ; c3                          ; 0xc26ce
  ; disGetNextSymbol 0xc26cf LB 0x140a -> off=0x0 cb=000000000000009c uValue=00000000000c26cf 'biosfn_write_string'
biosfn_write_string:                         ; 0xc26cf LB 0x9c
    push bp                                   ; 55                          ; 0xc26cf vgabios.c:1723
    mov bp, sp                                ; 89 e5                       ; 0xc26d0
    push si                                   ; 56                          ; 0xc26d2
    push di                                   ; 57                          ; 0xc26d3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc26d4
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc26d7
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc26da
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc26dd
    mov si, cx                                ; 89 ce                       ; 0xc26e0
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc26e2
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc26e5 vgabios.c:1730
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc26e8
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc26eb
    call 00a88h                               ; e8 97 e3                    ; 0xc26ee
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc26f1 vgabios.c:1733
    jne short 02708h                          ; 75 11                       ; 0xc26f5
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc26f7 vgabios.c:1734
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc26fa
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc26fd vgabios.c:1735
    xor al, al                                ; 30 c0                       ; 0xc2700
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2702
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2705
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc2708 vgabios.c:1738
    sal dx, 008h                              ; c1 e2 08                    ; 0xc270c
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc270f
    add dx, ax                                ; 01 c2                       ; 0xc2713
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2715 vgabios.c:1739
    call 00e5eh                               ; e8 42 e7                    ; 0xc2719
    dec si                                    ; 4e                          ; 0xc271c vgabios.c:1741
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc271d
    je short 02752h                           ; 74 30                       ; 0xc2720
    mov dx, di                                ; 89 fa                       ; 0xc2722 vgabios.c:1743
    inc di                                    ; 47                          ; 0xc2724
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc2725
    call 02f57h                               ; e8 2c 08                    ; 0xc2728
    mov cl, al                                ; 88 c1                       ; 0xc272b
    test byte [bp-00ah], 002h                 ; f6 46 f6 02                 ; 0xc272d vgabios.c:1744
    je short 0273fh                           ; 74 0c                       ; 0xc2731
    mov dx, di                                ; 89 fa                       ; 0xc2733 vgabios.c:1745
    inc di                                    ; 47                          ; 0xc2735
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc2736
    call 02f57h                               ; e8 1b 08                    ; 0xc2739
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc273c
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc273f vgabios.c:1747
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc2743
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc2747
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc274a
    call 0216ch                               ; e8 1c fa                    ; 0xc274d
    jmp short 0271ch                          ; eb ca                       ; 0xc2750 vgabios.c:1748
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc2752 vgabios.c:1751
    jne short 02762h                          ; 75 0a                       ; 0xc2756
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2758 vgabios.c:1752
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc275b
    call 00e5eh                               ; e8 fc e6                    ; 0xc275f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2762 vgabios.c:1753
    pop di                                    ; 5f                          ; 0xc2765
    pop si                                    ; 5e                          ; 0xc2766
    pop bp                                    ; 5d                          ; 0xc2767
    retn 00008h                               ; c2 08 00                    ; 0xc2768
  ; disGetNextSymbol 0xc276b LB 0x136e -> off=0x0 cb=0000000000000101 uValue=00000000000c276b 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc276b LB 0x101
    push bp                                   ; 55                          ; 0xc276b vgabios.c:1756
    mov bp, sp                                ; 89 e5                       ; 0xc276c
    push cx                                   ; 51                          ; 0xc276e
    push si                                   ; 56                          ; 0xc276f
    push di                                   ; 57                          ; 0xc2770
    push dx                                   ; 52                          ; 0xc2771
    push bx                                   ; 53                          ; 0xc2772
    mov cx, ds                                ; 8c d9                       ; 0xc2773 vgabios.c:1759
    mov bx, 05388h                            ; bb 88 53                    ; 0xc2775
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2778
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc277b
    call 02fa1h                               ; e8 20 08                    ; 0xc277e
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2781 vgabios.c:1762
    add di, strict byte 00004h                ; 83 c7 04                    ; 0xc2784
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2787
    mov si, strict word 00049h                ; be 49 00                    ; 0xc278a
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc278d
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc2790
    cld                                       ; fc                          ; 0xc2793
    jcxz 0279ch                               ; e3 06                       ; 0xc2794
    push DS                                   ; 1e                          ; 0xc2796
    mov ds, dx                                ; 8e da                       ; 0xc2797
    rep movsb                                 ; f3 a4                       ; 0xc2799
    pop DS                                    ; 1f                          ; 0xc279b
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc279c vgabios.c:1763
    add di, strict byte 00022h                ; 83 c7 22                    ; 0xc279f
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc27a2
    mov si, 00084h                            ; be 84 00                    ; 0xc27a5
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc27a8
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc27ab
    cld                                       ; fc                          ; 0xc27ae
    jcxz 027b7h                               ; e3 06                       ; 0xc27af
    push DS                                   ; 1e                          ; 0xc27b1
    mov ds, dx                                ; 8e da                       ; 0xc27b2
    rep movsb                                 ; f3 a4                       ; 0xc27b4
    pop DS                                    ; 1f                          ; 0xc27b6
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc27b7 vgabios.c:1765
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc27ba
    call 02f57h                               ; e8 97 07                    ; 0xc27bd
    movzx bx, al                              ; 0f b6 d8                    ; 0xc27c0
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc27c3
    add dx, strict byte 00025h                ; 83 c2 25                    ; 0xc27c6
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc27c9
    call 02f65h                               ; e8 96 07                    ; 0xc27cc
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc27cf vgabios.c:1766
    add dx, strict byte 00026h                ; 83 c2 26                    ; 0xc27d2
    xor bx, bx                                ; 31 db                       ; 0xc27d5
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc27d7
    call 02f65h                               ; e8 88 07                    ; 0xc27da
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc27dd vgabios.c:1767
    add dx, strict byte 00027h                ; 83 c2 27                    ; 0xc27e0
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc27e3
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc27e6
    call 02f65h                               ; e8 79 07                    ; 0xc27e9
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc27ec vgabios.c:1768
    add dx, strict byte 00028h                ; 83 c2 28                    ; 0xc27ef
    xor bx, bx                                ; 31 db                       ; 0xc27f2
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc27f4
    call 02f65h                               ; e8 6b 07                    ; 0xc27f7
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc27fa vgabios.c:1769
    add dx, strict byte 00029h                ; 83 c2 29                    ; 0xc27fd
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc2800
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2803
    call 02f65h                               ; e8 5c 07                    ; 0xc2806
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2809 vgabios.c:1770
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc280c
    mov bx, strict word 00002h                ; bb 02 00                    ; 0xc280f
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2812
    call 02f65h                               ; e8 4d 07                    ; 0xc2815
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2818 vgabios.c:1771
    add dx, strict byte 0002bh                ; 83 c2 2b                    ; 0xc281b
    xor bx, bx                                ; 31 db                       ; 0xc281e
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2820
    call 02f65h                               ; e8 3f 07                    ; 0xc2823
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2826 vgabios.c:1772
    add dx, strict byte 0002ch                ; 83 c2 2c                    ; 0xc2829
    xor bx, bx                                ; 31 db                       ; 0xc282c
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc282e
    call 02f65h                               ; e8 31 07                    ; 0xc2831
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2834 vgabios.c:1773
    add dx, strict byte 00031h                ; 83 c2 31                    ; 0xc2837
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc283a
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc283d
    call 02f65h                               ; e8 22 07                    ; 0xc2840
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2843 vgabios.c:1774
    add dx, strict byte 00032h                ; 83 c2 32                    ; 0xc2846
    xor bx, bx                                ; 31 db                       ; 0xc2849
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc284b
    call 02f65h                               ; e8 14 07                    ; 0xc284e
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2851 vgabios.c:1776
    add di, strict byte 00033h                ; 83 c7 33                    ; 0xc2854
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2857
    xor ax, ax                                ; 31 c0                       ; 0xc285a
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc285c
    cld                                       ; fc                          ; 0xc285f
    jcxz 02864h                               ; e3 02                       ; 0xc2860
    rep stosb                                 ; f3 aa                       ; 0xc2862
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2864 vgabios.c:1777
    pop di                                    ; 5f                          ; 0xc2867
    pop si                                    ; 5e                          ; 0xc2868
    pop cx                                    ; 59                          ; 0xc2869
    pop bp                                    ; 5d                          ; 0xc286a
    retn                                      ; c3                          ; 0xc286b
  ; disGetNextSymbol 0xc286c LB 0x126d -> off=0x0 cb=0000000000000023 uValue=00000000000c286c 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc286c LB 0x23
    push dx                                   ; 52                          ; 0xc286c vgabios.c:1780
    push bp                                   ; 55                          ; 0xc286d
    mov bp, sp                                ; 89 e5                       ; 0xc286e
    mov dx, ax                                ; 89 c2                       ; 0xc2870
    xor ax, ax                                ; 31 c0                       ; 0xc2872 vgabios.c:1784
    test dl, 001h                             ; f6 c2 01                    ; 0xc2874 vgabios.c:1785
    je short 0287ch                           ; 74 03                       ; 0xc2877
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2879 vgabios.c:1786
    test dl, 002h                             ; f6 c2 02                    ; 0xc287c vgabios.c:1788
    je short 02884h                           ; 74 03                       ; 0xc287f
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2881 vgabios.c:1789
    test dl, 004h                             ; f6 c2 04                    ; 0xc2884 vgabios.c:1791
    je short 0288ch                           ; 74 03                       ; 0xc2887
    add ax, 00304h                            ; 05 04 03                    ; 0xc2889 vgabios.c:1792
    pop bp                                    ; 5d                          ; 0xc288c vgabios.c:1796
    pop dx                                    ; 5a                          ; 0xc288d
    retn                                      ; c3                          ; 0xc288e
  ; disGetNextSymbol 0xc288f LB 0x124a -> off=0x0 cb=0000000000000012 uValue=00000000000c288f 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc288f LB 0x12
    push bp                                   ; 55                          ; 0xc288f vgabios.c:1798
    mov bp, sp                                ; 89 e5                       ; 0xc2890
    push bx                                   ; 53                          ; 0xc2892
    mov bx, dx                                ; 89 d3                       ; 0xc2893
    call 0286ch                               ; e8 d4 ff                    ; 0xc2895 vgabios.c:1800
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc2898
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc289b vgabios.c:1801
    pop bx                                    ; 5b                          ; 0xc289e
    pop bp                                    ; 5d                          ; 0xc289f
    retn                                      ; c3                          ; 0xc28a0
  ; disGetNextSymbol 0xc28a1 LB 0x1238 -> off=0x0 cb=0000000000000369 uValue=00000000000c28a1 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc28a1 LB 0x369
    push bp                                   ; 55                          ; 0xc28a1 vgabios.c:1803
    mov bp, sp                                ; 89 e5                       ; 0xc28a2
    push cx                                   ; 51                          ; 0xc28a4
    push si                                   ; 56                          ; 0xc28a5
    push di                                   ; 57                          ; 0xc28a6
    push ax                                   ; 50                          ; 0xc28a7
    push ax                                   ; 50                          ; 0xc28a8
    push ax                                   ; 50                          ; 0xc28a9
    mov si, dx                                ; 89 d6                       ; 0xc28aa
    mov cx, bx                                ; 89 d9                       ; 0xc28ac
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc28ae vgabios.c:1807
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28b1
    call 02f73h                               ; e8 bc 06                    ; 0xc28b4
    mov di, ax                                ; 89 c7                       ; 0xc28b7
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc28b9 vgabios.c:1808
    je near 02a24h                            ; 0f 84 63 01                 ; 0xc28bd
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc28c1 vgabios.c:1809
    in AL, DX                                 ; ec                          ; 0xc28c4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28c5
    movzx bx, al                              ; 0f b6 d8                    ; 0xc28c7
    mov dx, cx                                ; 89 ca                       ; 0xc28ca
    mov ax, si                                ; 89 f0                       ; 0xc28cc
    call 02f65h                               ; e8 94 06                    ; 0xc28ce
    inc cx                                    ; 41                          ; 0xc28d1 vgabios.c:1810
    mov dx, di                                ; 89 fa                       ; 0xc28d2
    in AL, DX                                 ; ec                          ; 0xc28d4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28d5
    movzx bx, al                              ; 0f b6 d8                    ; 0xc28d7
    mov dx, cx                                ; 89 ca                       ; 0xc28da
    mov ax, si                                ; 89 f0                       ; 0xc28dc
    call 02f65h                               ; e8 84 06                    ; 0xc28de
    inc cx                                    ; 41                          ; 0xc28e1 vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc28e2
    in AL, DX                                 ; ec                          ; 0xc28e5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28e6
    movzx bx, al                              ; 0f b6 d8                    ; 0xc28e8
    mov dx, cx                                ; 89 ca                       ; 0xc28eb
    mov ax, si                                ; 89 f0                       ; 0xc28ed
    call 02f65h                               ; e8 73 06                    ; 0xc28ef
    inc cx                                    ; 41                          ; 0xc28f2 vgabios.c:1812
    mov dx, 003dah                            ; ba da 03                    ; 0xc28f3
    in AL, DX                                 ; ec                          ; 0xc28f6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28f7
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc28f9 vgabios.c:1813
    in AL, DX                                 ; ec                          ; 0xc28fc
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28fd
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc28ff
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc2902 vgabios.c:1814
    mov dx, cx                                ; 89 ca                       ; 0xc2906
    mov ax, si                                ; 89 f0                       ; 0xc2908
    call 02f65h                               ; e8 58 06                    ; 0xc290a
    inc cx                                    ; 41                          ; 0xc290d vgabios.c:1815
    mov dx, 003cah                            ; ba ca 03                    ; 0xc290e
    in AL, DX                                 ; ec                          ; 0xc2911
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2912
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2914
    mov dx, cx                                ; 89 ca                       ; 0xc2917
    mov ax, si                                ; 89 f0                       ; 0xc2919
    call 02f65h                               ; e8 47 06                    ; 0xc291b
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc291e vgabios.c:1817
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2921
    add cx, ax                                ; 01 c1                       ; 0xc2924
    jmp short 0292eh                          ; eb 06                       ; 0xc2926
    cmp word [bp-00ah], strict byte 00004h    ; 83 7e f6 04                 ; 0xc2928
    jnbe short 0294bh                         ; 77 1d                       ; 0xc292c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc292e vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2931
    out DX, AL                                ; ee                          ; 0xc2934
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2935 vgabios.c:1819
    in AL, DX                                 ; ec                          ; 0xc2938
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2939
    movzx bx, al                              ; 0f b6 d8                    ; 0xc293b
    mov dx, cx                                ; 89 ca                       ; 0xc293e
    mov ax, si                                ; 89 f0                       ; 0xc2940
    call 02f65h                               ; e8 20 06                    ; 0xc2942
    inc cx                                    ; 41                          ; 0xc2945
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2946 vgabios.c:1820
    jmp short 02928h                          ; eb dd                       ; 0xc2949
    xor al, al                                ; 30 c0                       ; 0xc294b vgabios.c:1821
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc294d
    out DX, AL                                ; ee                          ; 0xc2950
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2951 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc2954
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2955
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2957
    mov dx, cx                                ; 89 ca                       ; 0xc295a
    mov ax, si                                ; 89 f0                       ; 0xc295c
    call 02f65h                               ; e8 04 06                    ; 0xc295e
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2961 vgabios.c:1824
    inc cx                                    ; 41                          ; 0xc2966
    jmp short 0296fh                          ; eb 06                       ; 0xc2967
    cmp word [bp-00ah], strict byte 00018h    ; 83 7e f6 18                 ; 0xc2969
    jnbe short 0298bh                         ; 77 1c                       ; 0xc296d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc296f vgabios.c:1825
    mov dx, di                                ; 89 fa                       ; 0xc2972
    out DX, AL                                ; ee                          ; 0xc2974
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2975 vgabios.c:1826
    in AL, DX                                 ; ec                          ; 0xc2978
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2979
    movzx bx, al                              ; 0f b6 d8                    ; 0xc297b
    mov dx, cx                                ; 89 ca                       ; 0xc297e
    mov ax, si                                ; 89 f0                       ; 0xc2980
    call 02f65h                               ; e8 e0 05                    ; 0xc2982
    inc cx                                    ; 41                          ; 0xc2985
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2986 vgabios.c:1827
    jmp short 02969h                          ; eb de                       ; 0xc2989
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc298b vgabios.c:1829
    jmp short 02998h                          ; eb 06                       ; 0xc2990
    cmp word [bp-00ah], strict byte 00013h    ; 83 7e f6 13                 ; 0xc2992
    jnbe short 029c1h                         ; 77 29                       ; 0xc2996
    mov dx, 003dah                            ; ba da 03                    ; 0xc2998 vgabios.c:1830
    in AL, DX                                 ; ec                          ; 0xc299b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc299c
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc299e vgabios.c:1831
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc29a1
    or ax, word [bp-00ah]                     ; 0b 46 f6                    ; 0xc29a4
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc29a7
    out DX, AL                                ; ee                          ; 0xc29aa
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc29ab vgabios.c:1832
    in AL, DX                                 ; ec                          ; 0xc29ae
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc29af
    movzx bx, al                              ; 0f b6 d8                    ; 0xc29b1
    mov dx, cx                                ; 89 ca                       ; 0xc29b4
    mov ax, si                                ; 89 f0                       ; 0xc29b6
    call 02f65h                               ; e8 aa 05                    ; 0xc29b8
    inc cx                                    ; 41                          ; 0xc29bb
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc29bc vgabios.c:1833
    jmp short 02992h                          ; eb d1                       ; 0xc29bf
    mov dx, 003dah                            ; ba da 03                    ; 0xc29c1 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc29c4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc29c5
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc29c7 vgabios.c:1836
    jmp short 029d4h                          ; eb 06                       ; 0xc29cc
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc29ce
    jnbe short 029f1h                         ; 77 1d                       ; 0xc29d2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc29d4 vgabios.c:1837
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc29d7
    out DX, AL                                ; ee                          ; 0xc29da
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc29db vgabios.c:1838
    in AL, DX                                 ; ec                          ; 0xc29de
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc29df
    movzx bx, al                              ; 0f b6 d8                    ; 0xc29e1
    mov dx, cx                                ; 89 ca                       ; 0xc29e4
    mov ax, si                                ; 89 f0                       ; 0xc29e6
    call 02f65h                               ; e8 7a 05                    ; 0xc29e8
    inc cx                                    ; 41                          ; 0xc29eb
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc29ec vgabios.c:1839
    jmp short 029ceh                          ; eb dd                       ; 0xc29ef
    mov bx, di                                ; 89 fb                       ; 0xc29f1 vgabios.c:1841
    mov dx, cx                                ; 89 ca                       ; 0xc29f3
    mov ax, si                                ; 89 f0                       ; 0xc29f5
    call 02f81h                               ; e8 87 05                    ; 0xc29f7
    inc cx                                    ; 41                          ; 0xc29fa vgabios.c:1844
    inc cx                                    ; 41                          ; 0xc29fb
    xor bx, bx                                ; 31 db                       ; 0xc29fc
    mov dx, cx                                ; 89 ca                       ; 0xc29fe
    mov ax, si                                ; 89 f0                       ; 0xc2a00
    call 02f65h                               ; e8 60 05                    ; 0xc2a02
    inc cx                                    ; 41                          ; 0xc2a05 vgabios.c:1845
    xor bx, bx                                ; 31 db                       ; 0xc2a06
    mov dx, cx                                ; 89 ca                       ; 0xc2a08
    mov ax, si                                ; 89 f0                       ; 0xc2a0a
    call 02f65h                               ; e8 56 05                    ; 0xc2a0c
    inc cx                                    ; 41                          ; 0xc2a0f vgabios.c:1846
    xor bx, bx                                ; 31 db                       ; 0xc2a10
    mov dx, cx                                ; 89 ca                       ; 0xc2a12
    mov ax, si                                ; 89 f0                       ; 0xc2a14
    call 02f65h                               ; e8 4c 05                    ; 0xc2a16
    inc cx                                    ; 41                          ; 0xc2a19 vgabios.c:1847
    xor bx, bx                                ; 31 db                       ; 0xc2a1a
    mov dx, cx                                ; 89 ca                       ; 0xc2a1c
    mov ax, si                                ; 89 f0                       ; 0xc2a1e
    call 02f65h                               ; e8 42 05                    ; 0xc2a20
    inc cx                                    ; 41                          ; 0xc2a23
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc2a24 vgabios.c:1849
    je near 02b93h                            ; 0f 84 67 01                 ; 0xc2a28
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2a2c vgabios.c:1850
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a2f
    call 02f57h                               ; e8 22 05                    ; 0xc2a32
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2a35
    mov dx, cx                                ; 89 ca                       ; 0xc2a38
    mov ax, si                                ; 89 f0                       ; 0xc2a3a
    call 02f65h                               ; e8 26 05                    ; 0xc2a3c
    inc cx                                    ; 41                          ; 0xc2a3f vgabios.c:1851
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2a40
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a43
    call 02f73h                               ; e8 2a 05                    ; 0xc2a46
    mov bx, ax                                ; 89 c3                       ; 0xc2a49
    mov dx, cx                                ; 89 ca                       ; 0xc2a4b
    mov ax, si                                ; 89 f0                       ; 0xc2a4d
    call 02f81h                               ; e8 2f 05                    ; 0xc2a4f
    inc cx                                    ; 41                          ; 0xc2a52 vgabios.c:1852
    inc cx                                    ; 41                          ; 0xc2a53
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2a54
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a57
    call 02f73h                               ; e8 16 05                    ; 0xc2a5a
    mov bx, ax                                ; 89 c3                       ; 0xc2a5d
    mov dx, cx                                ; 89 ca                       ; 0xc2a5f
    mov ax, si                                ; 89 f0                       ; 0xc2a61
    call 02f81h                               ; e8 1b 05                    ; 0xc2a63
    inc cx                                    ; 41                          ; 0xc2a66 vgabios.c:1853
    inc cx                                    ; 41                          ; 0xc2a67
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2a68
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a6b
    call 02f73h                               ; e8 02 05                    ; 0xc2a6e
    mov bx, ax                                ; 89 c3                       ; 0xc2a71
    mov dx, cx                                ; 89 ca                       ; 0xc2a73
    mov ax, si                                ; 89 f0                       ; 0xc2a75
    call 02f81h                               ; e8 07 05                    ; 0xc2a77
    inc cx                                    ; 41                          ; 0xc2a7a vgabios.c:1854
    inc cx                                    ; 41                          ; 0xc2a7b
    mov dx, 00084h                            ; ba 84 00                    ; 0xc2a7c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a7f
    call 02f57h                               ; e8 d2 04                    ; 0xc2a82
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2a85
    mov dx, cx                                ; 89 ca                       ; 0xc2a88
    mov ax, si                                ; 89 f0                       ; 0xc2a8a
    call 02f65h                               ; e8 d6 04                    ; 0xc2a8c
    inc cx                                    ; 41                          ; 0xc2a8f vgabios.c:1855
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2a90
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a93
    call 02f73h                               ; e8 da 04                    ; 0xc2a96
    mov bx, ax                                ; 89 c3                       ; 0xc2a99
    mov dx, cx                                ; 89 ca                       ; 0xc2a9b
    mov ax, si                                ; 89 f0                       ; 0xc2a9d
    call 02f81h                               ; e8 df 04                    ; 0xc2a9f
    inc cx                                    ; 41                          ; 0xc2aa2 vgabios.c:1856
    inc cx                                    ; 41                          ; 0xc2aa3
    mov dx, 00087h                            ; ba 87 00                    ; 0xc2aa4
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2aa7
    call 02f57h                               ; e8 aa 04                    ; 0xc2aaa
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2aad
    mov dx, cx                                ; 89 ca                       ; 0xc2ab0
    mov ax, si                                ; 89 f0                       ; 0xc2ab2
    call 02f65h                               ; e8 ae 04                    ; 0xc2ab4
    inc cx                                    ; 41                          ; 0xc2ab7 vgabios.c:1857
    mov dx, 00088h                            ; ba 88 00                    ; 0xc2ab8
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2abb
    call 02f57h                               ; e8 96 04                    ; 0xc2abe
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2ac1
    mov dx, cx                                ; 89 ca                       ; 0xc2ac4
    mov ax, si                                ; 89 f0                       ; 0xc2ac6
    call 02f65h                               ; e8 9a 04                    ; 0xc2ac8
    inc cx                                    ; 41                          ; 0xc2acb vgabios.c:1858
    mov dx, 00089h                            ; ba 89 00                    ; 0xc2acc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2acf
    call 02f57h                               ; e8 82 04                    ; 0xc2ad2
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2ad5
    mov dx, cx                                ; 89 ca                       ; 0xc2ad8
    mov ax, si                                ; 89 f0                       ; 0xc2ada
    call 02f65h                               ; e8 86 04                    ; 0xc2adc
    inc cx                                    ; 41                          ; 0xc2adf vgabios.c:1859
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc2ae0
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ae3
    call 02f73h                               ; e8 8a 04                    ; 0xc2ae6
    mov bx, ax                                ; 89 c3                       ; 0xc2ae9
    mov dx, cx                                ; 89 ca                       ; 0xc2aeb
    mov ax, si                                ; 89 f0                       ; 0xc2aed
    call 02f81h                               ; e8 8f 04                    ; 0xc2aef
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2af2 vgabios.c:1860
    inc cx                                    ; 41                          ; 0xc2af7
    inc cx                                    ; 41                          ; 0xc2af8
    jmp short 02b01h                          ; eb 06                       ; 0xc2af9
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2afb
    jnc short 02b1fh                          ; 73 1e                       ; 0xc2aff
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2b01 vgabios.c:1861
    add dx, dx                                ; 01 d2                       ; 0xc2b04
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc2b06
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b09
    call 02f73h                               ; e8 64 04                    ; 0xc2b0c
    mov bx, ax                                ; 89 c3                       ; 0xc2b0f
    mov dx, cx                                ; 89 ca                       ; 0xc2b11
    mov ax, si                                ; 89 f0                       ; 0xc2b13
    call 02f81h                               ; e8 69 04                    ; 0xc2b15
    inc cx                                    ; 41                          ; 0xc2b18 vgabios.c:1862
    inc cx                                    ; 41                          ; 0xc2b19
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2b1a vgabios.c:1863
    jmp short 02afbh                          ; eb dc                       ; 0xc2b1d
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc2b1f vgabios.c:1864
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b22
    call 02f73h                               ; e8 4b 04                    ; 0xc2b25
    mov bx, ax                                ; 89 c3                       ; 0xc2b28
    mov dx, cx                                ; 89 ca                       ; 0xc2b2a
    mov ax, si                                ; 89 f0                       ; 0xc2b2c
    call 02f81h                               ; e8 50 04                    ; 0xc2b2e
    inc cx                                    ; 41                          ; 0xc2b31 vgabios.c:1865
    inc cx                                    ; 41                          ; 0xc2b32
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2b33
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b36
    call 02f57h                               ; e8 1b 04                    ; 0xc2b39
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2b3c
    mov dx, cx                                ; 89 ca                       ; 0xc2b3f
    mov ax, si                                ; 89 f0                       ; 0xc2b41
    call 02f65h                               ; e8 1f 04                    ; 0xc2b43
    inc cx                                    ; 41                          ; 0xc2b46 vgabios.c:1867
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc2b47
    xor ax, ax                                ; 31 c0                       ; 0xc2b4a
    call 02f73h                               ; e8 24 04                    ; 0xc2b4c
    mov bx, ax                                ; 89 c3                       ; 0xc2b4f
    mov dx, cx                                ; 89 ca                       ; 0xc2b51
    mov ax, si                                ; 89 f0                       ; 0xc2b53
    call 02f81h                               ; e8 29 04                    ; 0xc2b55
    inc cx                                    ; 41                          ; 0xc2b58 vgabios.c:1868
    inc cx                                    ; 41                          ; 0xc2b59
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc2b5a
    xor ax, ax                                ; 31 c0                       ; 0xc2b5d
    call 02f73h                               ; e8 11 04                    ; 0xc2b5f
    mov bx, ax                                ; 89 c3                       ; 0xc2b62
    mov dx, cx                                ; 89 ca                       ; 0xc2b64
    mov ax, si                                ; 89 f0                       ; 0xc2b66
    call 02f81h                               ; e8 16 04                    ; 0xc2b68
    inc cx                                    ; 41                          ; 0xc2b6b vgabios.c:1869
    inc cx                                    ; 41                          ; 0xc2b6c
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc2b6d
    xor ax, ax                                ; 31 c0                       ; 0xc2b70
    call 02f73h                               ; e8 fe 03                    ; 0xc2b72
    mov bx, ax                                ; 89 c3                       ; 0xc2b75
    mov dx, cx                                ; 89 ca                       ; 0xc2b77
    mov ax, si                                ; 89 f0                       ; 0xc2b79
    call 02f81h                               ; e8 03 04                    ; 0xc2b7b
    inc cx                                    ; 41                          ; 0xc2b7e vgabios.c:1870
    inc cx                                    ; 41                          ; 0xc2b7f
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc2b80
    xor ax, ax                                ; 31 c0                       ; 0xc2b83
    call 02f73h                               ; e8 eb 03                    ; 0xc2b85
    mov bx, ax                                ; 89 c3                       ; 0xc2b88
    mov dx, cx                                ; 89 ca                       ; 0xc2b8a
    mov ax, si                                ; 89 f0                       ; 0xc2b8c
    call 02f81h                               ; e8 f0 03                    ; 0xc2b8e
    inc cx                                    ; 41                          ; 0xc2b91
    inc cx                                    ; 41                          ; 0xc2b92
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc2b93 vgabios.c:1872
    je short 02c00h                           ; 74 67                       ; 0xc2b97
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc2b99 vgabios.c:1874
    in AL, DX                                 ; ec                          ; 0xc2b9c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b9d
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2b9f
    mov dx, cx                                ; 89 ca                       ; 0xc2ba2
    mov ax, si                                ; 89 f0                       ; 0xc2ba4
    call 02f65h                               ; e8 bc 03                    ; 0xc2ba6
    inc cx                                    ; 41                          ; 0xc2ba9 vgabios.c:1875
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2baa
    in AL, DX                                 ; ec                          ; 0xc2bad
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bae
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2bb0
    mov dx, cx                                ; 89 ca                       ; 0xc2bb3
    mov ax, si                                ; 89 f0                       ; 0xc2bb5
    call 02f65h                               ; e8 ab 03                    ; 0xc2bb7
    inc cx                                    ; 41                          ; 0xc2bba vgabios.c:1876
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc2bbb
    in AL, DX                                 ; ec                          ; 0xc2bbe
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bbf
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2bc1
    mov dx, cx                                ; 89 ca                       ; 0xc2bc4
    mov ax, si                                ; 89 f0                       ; 0xc2bc6
    call 02f65h                               ; e8 9a 03                    ; 0xc2bc8
    inc cx                                    ; 41                          ; 0xc2bcb vgabios.c:1878
    xor al, al                                ; 30 c0                       ; 0xc2bcc
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2bce
    out DX, AL                                ; ee                          ; 0xc2bd1
    xor ah, ah                                ; 30 e4                       ; 0xc2bd2 vgabios.c:1879
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2bd4
    jmp short 02be0h                          ; eb 07                       ; 0xc2bd7
    cmp word [bp-00ah], 00300h                ; 81 7e f6 00 03              ; 0xc2bd9
    jnc short 02bf6h                          ; 73 16                       ; 0xc2bde
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc2be0 vgabios.c:1880
    in AL, DX                                 ; ec                          ; 0xc2be3
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2be4
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2be6
    mov dx, cx                                ; 89 ca                       ; 0xc2be9
    mov ax, si                                ; 89 f0                       ; 0xc2beb
    call 02f65h                               ; e8 75 03                    ; 0xc2bed
    inc cx                                    ; 41                          ; 0xc2bf0
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2bf1 vgabios.c:1881
    jmp short 02bd9h                          ; eb e3                       ; 0xc2bf4
    xor bx, bx                                ; 31 db                       ; 0xc2bf6 vgabios.c:1882
    mov dx, cx                                ; 89 ca                       ; 0xc2bf8
    mov ax, si                                ; 89 f0                       ; 0xc2bfa
    call 02f65h                               ; e8 66 03                    ; 0xc2bfc
    inc cx                                    ; 41                          ; 0xc2bff
    mov ax, cx                                ; 89 c8                       ; 0xc2c00 vgabios.c:1885
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2c02
    pop di                                    ; 5f                          ; 0xc2c05
    pop si                                    ; 5e                          ; 0xc2c06
    pop cx                                    ; 59                          ; 0xc2c07
    pop bp                                    ; 5d                          ; 0xc2c08
    retn                                      ; c3                          ; 0xc2c09
  ; disGetNextSymbol 0xc2c0a LB 0xecf -> off=0x0 cb=0000000000000326 uValue=00000000000c2c0a 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc2c0a LB 0x326
    push bp                                   ; 55                          ; 0xc2c0a vgabios.c:1887
    mov bp, sp                                ; 89 e5                       ; 0xc2c0b
    push cx                                   ; 51                          ; 0xc2c0d
    push si                                   ; 56                          ; 0xc2c0e
    push di                                   ; 57                          ; 0xc2c0f
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc2c10
    push ax                                   ; 50                          ; 0xc2c13
    mov si, dx                                ; 89 d6                       ; 0xc2c14
    mov cx, bx                                ; 89 d9                       ; 0xc2c16
    test byte [bp-00eh], 001h                 ; f6 46 f2 01                 ; 0xc2c18 vgabios.c:1891
    je near 02d6ah                            ; 0f 84 4a 01                 ; 0xc2c1c
    mov dx, 003dah                            ; ba da 03                    ; 0xc2c20 vgabios.c:1893
    in AL, DX                                 ; ec                          ; 0xc2c23
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2c24
    lea dx, [bx+040h]                         ; 8d 57 40                    ; 0xc2c26 vgabios.c:1895
    mov ax, si                                ; 89 f0                       ; 0xc2c29
    call 02f73h                               ; e8 45 03                    ; 0xc2c2b
    mov di, ax                                ; 89 c7                       ; 0xc2c2e
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc2c30 vgabios.c:1899
    lea cx, [bx+005h]                         ; 8d 4f 05                    ; 0xc2c35 vgabios.c:1897
    jmp short 02c40h                          ; eb 06                       ; 0xc2c38
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc2c3a
    jnbe short 02c58h                         ; 77 18                       ; 0xc2c3e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c40 vgabios.c:1900
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2c43
    out DX, AL                                ; ee                          ; 0xc2c46
    mov dx, cx                                ; 89 ca                       ; 0xc2c47 vgabios.c:1901
    mov ax, si                                ; 89 f0                       ; 0xc2c49
    call 02f57h                               ; e8 09 03                    ; 0xc2c4b
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2c4e
    out DX, AL                                ; ee                          ; 0xc2c51
    inc cx                                    ; 41                          ; 0xc2c52
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2c53 vgabios.c:1902
    jmp short 02c3ah                          ; eb e2                       ; 0xc2c56
    xor al, al                                ; 30 c0                       ; 0xc2c58 vgabios.c:1903
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2c5a
    out DX, AL                                ; ee                          ; 0xc2c5d
    mov dx, cx                                ; 89 ca                       ; 0xc2c5e vgabios.c:1904
    mov ax, si                                ; 89 f0                       ; 0xc2c60
    call 02f57h                               ; e8 f2 02                    ; 0xc2c62
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2c65
    out DX, AL                                ; ee                          ; 0xc2c68
    inc cx                                    ; 41                          ; 0xc2c69 vgabios.c:1907
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc2c6a
    mov dx, di                                ; 89 fa                       ; 0xc2c6d
    out DX, ax                                ; ef                          ; 0xc2c6f
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2c70 vgabios.c:1909
    jmp short 02c7dh                          ; eb 06                       ; 0xc2c75
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc2c77
    jnbe short 02c9ah                         ; 77 1d                       ; 0xc2c7b
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc2c7d vgabios.c:1910
    je short 02c94h                           ; 74 11                       ; 0xc2c81
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c83 vgabios.c:1911
    mov dx, di                                ; 89 fa                       ; 0xc2c86
    out DX, AL                                ; ee                          ; 0xc2c88
    mov dx, cx                                ; 89 ca                       ; 0xc2c89 vgabios.c:1912
    mov ax, si                                ; 89 f0                       ; 0xc2c8b
    call 02f57h                               ; e8 c7 02                    ; 0xc2c8d
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2c90
    out DX, AL                                ; ee                          ; 0xc2c93
    inc cx                                    ; 41                          ; 0xc2c94 vgabios.c:1914
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2c95 vgabios.c:1915
    jmp short 02c77h                          ; eb dd                       ; 0xc2c98
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2c9a vgabios.c:1917
    in AL, DX                                 ; ec                          ; 0xc2c9d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2c9e
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc2ca0
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2ca2
    cmp di, 003d4h                            ; 81 ff d4 03                 ; 0xc2ca5 vgabios.c:1918
    jne short 02cafh                          ; 75 04                       ; 0xc2ca9
    or byte [bp-00ah], 001h                   ; 80 4e f6 01                 ; 0xc2cab vgabios.c:1919
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2caf vgabios.c:1920
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc2cb2
    out DX, AL                                ; ee                          ; 0xc2cb5
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc2cb6 vgabios.c:1923
    mov dx, di                                ; 89 fa                       ; 0xc2cb8
    out DX, AL                                ; ee                          ; 0xc2cba
    mov dx, cx                                ; 89 ca                       ; 0xc2cbb vgabios.c:1924
    add dx, strict byte 0fff9h                ; 83 c2 f9                    ; 0xc2cbd
    mov ax, si                                ; 89 f0                       ; 0xc2cc0
    call 02f57h                               ; e8 92 02                    ; 0xc2cc2
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2cc5
    out DX, AL                                ; ee                          ; 0xc2cc8
    lea dx, [bx+003h]                         ; 8d 57 03                    ; 0xc2cc9 vgabios.c:1927
    mov ax, si                                ; 89 f0                       ; 0xc2ccc
    call 02f57h                               ; e8 86 02                    ; 0xc2cce
    xor ah, ah                                ; 30 e4                       ; 0xc2cd1
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc2cd3
    mov dx, 003dah                            ; ba da 03                    ; 0xc2cd6 vgabios.c:1928
    in AL, DX                                 ; ec                          ; 0xc2cd9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2cda
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2cdc vgabios.c:1929
    jmp short 02ce9h                          ; eb 06                       ; 0xc2ce1
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc2ce3
    jnbe short 02d07h                         ; 77 1e                       ; 0xc2ce7
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc2ce9 vgabios.c:1930
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2cec
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc2cef
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2cf2
    out DX, AL                                ; ee                          ; 0xc2cf5
    mov dx, cx                                ; 89 ca                       ; 0xc2cf6 vgabios.c:1931
    mov ax, si                                ; 89 f0                       ; 0xc2cf8
    call 02f57h                               ; e8 5a 02                    ; 0xc2cfa
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2cfd
    out DX, AL                                ; ee                          ; 0xc2d00
    inc cx                                    ; 41                          ; 0xc2d01
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2d02 vgabios.c:1932
    jmp short 02ce3h                          ; eb dc                       ; 0xc2d05
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2d07 vgabios.c:1933
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2d0a
    out DX, AL                                ; ee                          ; 0xc2d0d
    mov dx, 003dah                            ; ba da 03                    ; 0xc2d0e vgabios.c:1934
    in AL, DX                                 ; ec                          ; 0xc2d11
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d12
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2d14 vgabios.c:1936
    jmp short 02d21h                          ; eb 06                       ; 0xc2d19
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc2d1b
    jnbe short 02d39h                         ; 77 18                       ; 0xc2d1f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d21 vgabios.c:1937
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2d24
    out DX, AL                                ; ee                          ; 0xc2d27
    mov dx, cx                                ; 89 ca                       ; 0xc2d28 vgabios.c:1938
    mov ax, si                                ; 89 f0                       ; 0xc2d2a
    call 02f57h                               ; e8 28 02                    ; 0xc2d2c
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2d2f
    out DX, AL                                ; ee                          ; 0xc2d32
    inc cx                                    ; 41                          ; 0xc2d33
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2d34 vgabios.c:1939
    jmp short 02d1bh                          ; eb e2                       ; 0xc2d37
    add cx, strict byte 00006h                ; 83 c1 06                    ; 0xc2d39 vgabios.c:1940
    mov dx, bx                                ; 89 da                       ; 0xc2d3c vgabios.c:1941
    mov ax, si                                ; 89 f0                       ; 0xc2d3e
    call 02f57h                               ; e8 14 02                    ; 0xc2d40
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2d43
    out DX, AL                                ; ee                          ; 0xc2d46
    inc bx                                    ; 43                          ; 0xc2d47
    mov dx, bx                                ; 89 da                       ; 0xc2d48 vgabios.c:1944
    mov ax, si                                ; 89 f0                       ; 0xc2d4a
    call 02f57h                               ; e8 08 02                    ; 0xc2d4c
    mov dx, di                                ; 89 fa                       ; 0xc2d4f
    out DX, AL                                ; ee                          ; 0xc2d51
    inc bx                                    ; 43                          ; 0xc2d52
    mov dx, bx                                ; 89 da                       ; 0xc2d53 vgabios.c:1945
    mov ax, si                                ; 89 f0                       ; 0xc2d55
    call 02f57h                               ; e8 fd 01                    ; 0xc2d57
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2d5a
    out DX, AL                                ; ee                          ; 0xc2d5d
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc2d5e
    mov ax, si                                ; 89 f0                       ; 0xc2d61
    call 02f57h                               ; e8 f1 01                    ; 0xc2d63
    lea dx, [di+006h]                         ; 8d 55 06                    ; 0xc2d66
    out DX, AL                                ; ee                          ; 0xc2d69
    test byte [bp-00eh], 002h                 ; f6 46 f2 02                 ; 0xc2d6a vgabios.c:1949
    je near 02ed9h                            ; 0f 84 67 01                 ; 0xc2d6e
    mov dx, cx                                ; 89 ca                       ; 0xc2d72 vgabios.c:1950
    mov ax, si                                ; 89 f0                       ; 0xc2d74
    call 02f57h                               ; e8 de 01                    ; 0xc2d76
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2d79
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2d7c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d7f
    call 02f65h                               ; e8 e0 01                    ; 0xc2d82
    inc cx                                    ; 41                          ; 0xc2d85
    mov dx, cx                                ; 89 ca                       ; 0xc2d86 vgabios.c:1951
    mov ax, si                                ; 89 f0                       ; 0xc2d88
    call 02f73h                               ; e8 e6 01                    ; 0xc2d8a
    mov bx, ax                                ; 89 c3                       ; 0xc2d8d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2d8f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d92
    call 02f81h                               ; e8 e9 01                    ; 0xc2d95
    inc cx                                    ; 41                          ; 0xc2d98
    inc cx                                    ; 41                          ; 0xc2d99
    mov dx, cx                                ; 89 ca                       ; 0xc2d9a vgabios.c:1952
    mov ax, si                                ; 89 f0                       ; 0xc2d9c
    call 02f73h                               ; e8 d2 01                    ; 0xc2d9e
    mov bx, ax                                ; 89 c3                       ; 0xc2da1
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2da3
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2da6
    call 02f81h                               ; e8 d5 01                    ; 0xc2da9
    inc cx                                    ; 41                          ; 0xc2dac
    inc cx                                    ; 41                          ; 0xc2dad
    mov dx, cx                                ; 89 ca                       ; 0xc2dae vgabios.c:1953
    mov ax, si                                ; 89 f0                       ; 0xc2db0
    call 02f73h                               ; e8 be 01                    ; 0xc2db2
    mov bx, ax                                ; 89 c3                       ; 0xc2db5
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2db7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2dba
    call 02f81h                               ; e8 c1 01                    ; 0xc2dbd
    inc cx                                    ; 41                          ; 0xc2dc0
    inc cx                                    ; 41                          ; 0xc2dc1
    mov dx, cx                                ; 89 ca                       ; 0xc2dc2 vgabios.c:1954
    mov ax, si                                ; 89 f0                       ; 0xc2dc4
    call 02f57h                               ; e8 8e 01                    ; 0xc2dc6
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2dc9
    mov dx, 00084h                            ; ba 84 00                    ; 0xc2dcc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2dcf
    call 02f65h                               ; e8 90 01                    ; 0xc2dd2
    inc cx                                    ; 41                          ; 0xc2dd5
    mov dx, cx                                ; 89 ca                       ; 0xc2dd6 vgabios.c:1955
    mov ax, si                                ; 89 f0                       ; 0xc2dd8
    call 02f73h                               ; e8 96 01                    ; 0xc2dda
    mov bx, ax                                ; 89 c3                       ; 0xc2ddd
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2ddf
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2de2
    call 02f81h                               ; e8 99 01                    ; 0xc2de5
    inc cx                                    ; 41                          ; 0xc2de8
    inc cx                                    ; 41                          ; 0xc2de9
    mov dx, cx                                ; 89 ca                       ; 0xc2dea vgabios.c:1956
    mov ax, si                                ; 89 f0                       ; 0xc2dec
    call 02f57h                               ; e8 66 01                    ; 0xc2dee
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2df1
    mov dx, 00087h                            ; ba 87 00                    ; 0xc2df4
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2df7
    call 02f65h                               ; e8 68 01                    ; 0xc2dfa
    inc cx                                    ; 41                          ; 0xc2dfd
    mov dx, cx                                ; 89 ca                       ; 0xc2dfe vgabios.c:1957
    mov ax, si                                ; 89 f0                       ; 0xc2e00
    call 02f57h                               ; e8 52 01                    ; 0xc2e02
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2e05
    mov dx, 00088h                            ; ba 88 00                    ; 0xc2e08
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e0b
    call 02f65h                               ; e8 54 01                    ; 0xc2e0e
    inc cx                                    ; 41                          ; 0xc2e11
    mov dx, cx                                ; 89 ca                       ; 0xc2e12 vgabios.c:1958
    mov ax, si                                ; 89 f0                       ; 0xc2e14
    call 02f57h                               ; e8 3e 01                    ; 0xc2e16
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2e19
    mov dx, 00089h                            ; ba 89 00                    ; 0xc2e1c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e1f
    call 02f65h                               ; e8 40 01                    ; 0xc2e22
    inc cx                                    ; 41                          ; 0xc2e25
    mov dx, cx                                ; 89 ca                       ; 0xc2e26 vgabios.c:1959
    mov ax, si                                ; 89 f0                       ; 0xc2e28
    call 02f73h                               ; e8 46 01                    ; 0xc2e2a
    mov bx, ax                                ; 89 c3                       ; 0xc2e2d
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc2e2f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e32
    call 02f81h                               ; e8 49 01                    ; 0xc2e35
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2e38 vgabios.c:1960
    inc cx                                    ; 41                          ; 0xc2e3d
    inc cx                                    ; 41                          ; 0xc2e3e
    jmp short 02e47h                          ; eb 06                       ; 0xc2e3f
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc2e41
    jnc short 02e65h                          ; 73 1e                       ; 0xc2e45
    mov dx, cx                                ; 89 ca                       ; 0xc2e47 vgabios.c:1961
    mov ax, si                                ; 89 f0                       ; 0xc2e49
    call 02f73h                               ; e8 25 01                    ; 0xc2e4b
    mov bx, ax                                ; 89 c3                       ; 0xc2e4e
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc2e50
    add dx, dx                                ; 01 d2                       ; 0xc2e53
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc2e55
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e58
    call 02f81h                               ; e8 23 01                    ; 0xc2e5b
    inc cx                                    ; 41                          ; 0xc2e5e vgabios.c:1962
    inc cx                                    ; 41                          ; 0xc2e5f
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2e60 vgabios.c:1963
    jmp short 02e41h                          ; eb dc                       ; 0xc2e63
    mov dx, cx                                ; 89 ca                       ; 0xc2e65 vgabios.c:1964
    mov ax, si                                ; 89 f0                       ; 0xc2e67
    call 02f73h                               ; e8 07 01                    ; 0xc2e69
    mov bx, ax                                ; 89 c3                       ; 0xc2e6c
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc2e6e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e71
    call 02f81h                               ; e8 0a 01                    ; 0xc2e74
    inc cx                                    ; 41                          ; 0xc2e77
    inc cx                                    ; 41                          ; 0xc2e78
    mov dx, cx                                ; 89 ca                       ; 0xc2e79 vgabios.c:1965
    mov ax, si                                ; 89 f0                       ; 0xc2e7b
    call 02f57h                               ; e8 d7 00                    ; 0xc2e7d
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2e80
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2e83
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e86
    call 02f65h                               ; e8 d9 00                    ; 0xc2e89
    inc cx                                    ; 41                          ; 0xc2e8c
    mov dx, cx                                ; 89 ca                       ; 0xc2e8d vgabios.c:1967
    mov ax, si                                ; 89 f0                       ; 0xc2e8f
    call 02f73h                               ; e8 df 00                    ; 0xc2e91
    mov bx, ax                                ; 89 c3                       ; 0xc2e94
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc2e96
    xor ax, ax                                ; 31 c0                       ; 0xc2e99
    call 02f81h                               ; e8 e3 00                    ; 0xc2e9b
    inc cx                                    ; 41                          ; 0xc2e9e
    inc cx                                    ; 41                          ; 0xc2e9f
    mov dx, cx                                ; 89 ca                       ; 0xc2ea0 vgabios.c:1968
    mov ax, si                                ; 89 f0                       ; 0xc2ea2
    call 02f73h                               ; e8 cc 00                    ; 0xc2ea4
    mov bx, ax                                ; 89 c3                       ; 0xc2ea7
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc2ea9
    xor ax, ax                                ; 31 c0                       ; 0xc2eac
    call 02f81h                               ; e8 d0 00                    ; 0xc2eae
    inc cx                                    ; 41                          ; 0xc2eb1
    inc cx                                    ; 41                          ; 0xc2eb2
    mov dx, cx                                ; 89 ca                       ; 0xc2eb3 vgabios.c:1969
    mov ax, si                                ; 89 f0                       ; 0xc2eb5
    call 02f73h                               ; e8 b9 00                    ; 0xc2eb7
    mov bx, ax                                ; 89 c3                       ; 0xc2eba
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc2ebc
    xor ax, ax                                ; 31 c0                       ; 0xc2ebf
    call 02f81h                               ; e8 bd 00                    ; 0xc2ec1
    inc cx                                    ; 41                          ; 0xc2ec4
    inc cx                                    ; 41                          ; 0xc2ec5
    mov dx, cx                                ; 89 ca                       ; 0xc2ec6 vgabios.c:1970
    mov ax, si                                ; 89 f0                       ; 0xc2ec8
    call 02f73h                               ; e8 a6 00                    ; 0xc2eca
    mov bx, ax                                ; 89 c3                       ; 0xc2ecd
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc2ecf
    xor ax, ax                                ; 31 c0                       ; 0xc2ed2
    call 02f81h                               ; e8 aa 00                    ; 0xc2ed4
    inc cx                                    ; 41                          ; 0xc2ed7
    inc cx                                    ; 41                          ; 0xc2ed8
    test byte [bp-00eh], 004h                 ; f6 46 f2 04                 ; 0xc2ed9 vgabios.c:1972
    je short 02f26h                           ; 74 47                       ; 0xc2edd
    inc cx                                    ; 41                          ; 0xc2edf vgabios.c:1973
    mov dx, cx                                ; 89 ca                       ; 0xc2ee0 vgabios.c:1974
    mov ax, si                                ; 89 f0                       ; 0xc2ee2
    call 02f57h                               ; e8 70 00                    ; 0xc2ee4
    xor ah, ah                                ; 30 e4                       ; 0xc2ee7
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2ee9
    inc cx                                    ; 41                          ; 0xc2eec
    mov dx, cx                                ; 89 ca                       ; 0xc2eed vgabios.c:1975
    mov ax, si                                ; 89 f0                       ; 0xc2eef
    call 02f57h                               ; e8 63 00                    ; 0xc2ef1
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc2ef4
    out DX, AL                                ; ee                          ; 0xc2ef7
    inc cx                                    ; 41                          ; 0xc2ef8 vgabios.c:1977
    xor al, al                                ; 30 c0                       ; 0xc2ef9
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2efb
    out DX, AL                                ; ee                          ; 0xc2efe
    xor ah, ah                                ; 30 e4                       ; 0xc2eff vgabios.c:1978
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2f01
    jmp short 02f0dh                          ; eb 07                       ; 0xc2f04
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc2f06
    jnc short 02f1eh                          ; 73 11                       ; 0xc2f0b
    mov dx, cx                                ; 89 ca                       ; 0xc2f0d vgabios.c:1979
    mov ax, si                                ; 89 f0                       ; 0xc2f0f
    call 02f57h                               ; e8 43 00                    ; 0xc2f11
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc2f14
    out DX, AL                                ; ee                          ; 0xc2f17
    inc cx                                    ; 41                          ; 0xc2f18
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2f19 vgabios.c:1980
    jmp short 02f06h                          ; eb e8                       ; 0xc2f1c
    inc cx                                    ; 41                          ; 0xc2f1e vgabios.c:1981
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2f1f
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2f22
    out DX, AL                                ; ee                          ; 0xc2f25
    mov ax, cx                                ; 89 c8                       ; 0xc2f26 vgabios.c:1985
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2f28
    pop di                                    ; 5f                          ; 0xc2f2b
    pop si                                    ; 5e                          ; 0xc2f2c
    pop cx                                    ; 59                          ; 0xc2f2d
    pop bp                                    ; 5d                          ; 0xc2f2e
    retn                                      ; c3                          ; 0xc2f2f
  ; disGetNextSymbol 0xc2f30 LB 0xba9 -> off=0x0 cb=0000000000000027 uValue=00000000000c2f30 'find_vga_entry'
find_vga_entry:                              ; 0xc2f30 LB 0x27
    push bx                                   ; 53                          ; 0xc2f30 vgabios.c:1994
    push dx                                   ; 52                          ; 0xc2f31
    push bp                                   ; 55                          ; 0xc2f32
    mov bp, sp                                ; 89 e5                       ; 0xc2f33
    mov dl, al                                ; 88 c2                       ; 0xc2f35
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc2f37 vgabios.c:1996
    xor al, al                                ; 30 c0                       ; 0xc2f39 vgabios.c:1997
    jmp short 02f43h                          ; eb 06                       ; 0xc2f3b
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2f3d vgabios.c:1998
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc2f3f
    jnbe short 02f51h                         ; 77 0e                       ; 0xc2f41
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2f43
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2f46
    cmp dl, byte [bx+04634h]                  ; 3a 97 34 46                 ; 0xc2f49
    jne short 02f3dh                          ; 75 ee                       ; 0xc2f4d
    mov ah, al                                ; 88 c4                       ; 0xc2f4f
    mov al, ah                                ; 88 e0                       ; 0xc2f51 vgabios.c:2003
    pop bp                                    ; 5d                          ; 0xc2f53
    pop dx                                    ; 5a                          ; 0xc2f54
    pop bx                                    ; 5b                          ; 0xc2f55
    retn                                      ; c3                          ; 0xc2f56
  ; disGetNextSymbol 0xc2f57 LB 0xb82 -> off=0x0 cb=000000000000000e uValue=00000000000c2f57 'read_byte'
read_byte:                                   ; 0xc2f57 LB 0xe
    push bx                                   ; 53                          ; 0xc2f57 vgabios.c:2011
    push bp                                   ; 55                          ; 0xc2f58
    mov bp, sp                                ; 89 e5                       ; 0xc2f59
    mov bx, dx                                ; 89 d3                       ; 0xc2f5b
    mov es, ax                                ; 8e c0                       ; 0xc2f5d vgabios.c:2013
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2f5f
    pop bp                                    ; 5d                          ; 0xc2f62 vgabios.c:2014
    pop bx                                    ; 5b                          ; 0xc2f63
    retn                                      ; c3                          ; 0xc2f64
  ; disGetNextSymbol 0xc2f65 LB 0xb74 -> off=0x0 cb=000000000000000e uValue=00000000000c2f65 'write_byte'
write_byte:                                  ; 0xc2f65 LB 0xe
    push si                                   ; 56                          ; 0xc2f65 vgabios.c:2016
    push bp                                   ; 55                          ; 0xc2f66
    mov bp, sp                                ; 89 e5                       ; 0xc2f67
    mov si, dx                                ; 89 d6                       ; 0xc2f69
    mov es, ax                                ; 8e c0                       ; 0xc2f6b vgabios.c:2018
    mov byte [es:si], bl                      ; 26 88 1c                    ; 0xc2f6d
    pop bp                                    ; 5d                          ; 0xc2f70 vgabios.c:2019
    pop si                                    ; 5e                          ; 0xc2f71
    retn                                      ; c3                          ; 0xc2f72
  ; disGetNextSymbol 0xc2f73 LB 0xb66 -> off=0x0 cb=000000000000000e uValue=00000000000c2f73 'read_word'
read_word:                                   ; 0xc2f73 LB 0xe
    push bx                                   ; 53                          ; 0xc2f73 vgabios.c:2021
    push bp                                   ; 55                          ; 0xc2f74
    mov bp, sp                                ; 89 e5                       ; 0xc2f75
    mov bx, dx                                ; 89 d3                       ; 0xc2f77
    mov es, ax                                ; 8e c0                       ; 0xc2f79 vgabios.c:2023
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2f7b
    pop bp                                    ; 5d                          ; 0xc2f7e vgabios.c:2024
    pop bx                                    ; 5b                          ; 0xc2f7f
    retn                                      ; c3                          ; 0xc2f80
  ; disGetNextSymbol 0xc2f81 LB 0xb58 -> off=0x0 cb=000000000000000e uValue=00000000000c2f81 'write_word'
write_word:                                  ; 0xc2f81 LB 0xe
    push si                                   ; 56                          ; 0xc2f81 vgabios.c:2026
    push bp                                   ; 55                          ; 0xc2f82
    mov bp, sp                                ; 89 e5                       ; 0xc2f83
    mov si, dx                                ; 89 d6                       ; 0xc2f85
    mov es, ax                                ; 8e c0                       ; 0xc2f87 vgabios.c:2028
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2f89
    pop bp                                    ; 5d                          ; 0xc2f8c vgabios.c:2029
    pop si                                    ; 5e                          ; 0xc2f8d
    retn                                      ; c3                          ; 0xc2f8e
  ; disGetNextSymbol 0xc2f8f LB 0xb4a -> off=0x0 cb=0000000000000012 uValue=00000000000c2f8f 'read_dword'
read_dword:                                  ; 0xc2f8f LB 0x12
    push bx                                   ; 53                          ; 0xc2f8f vgabios.c:2031
    push bp                                   ; 55                          ; 0xc2f90
    mov bp, sp                                ; 89 e5                       ; 0xc2f91
    mov bx, dx                                ; 89 d3                       ; 0xc2f93
    mov es, ax                                ; 8e c0                       ; 0xc2f95 vgabios.c:2033
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2f97
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc2f9a
    pop bp                                    ; 5d                          ; 0xc2f9e vgabios.c:2034
    pop bx                                    ; 5b                          ; 0xc2f9f
    retn                                      ; c3                          ; 0xc2fa0
  ; disGetNextSymbol 0xc2fa1 LB 0xb38 -> off=0x0 cb=0000000000000012 uValue=00000000000c2fa1 'write_dword'
write_dword:                                 ; 0xc2fa1 LB 0x12
    push si                                   ; 56                          ; 0xc2fa1 vgabios.c:2036
    push bp                                   ; 55                          ; 0xc2fa2
    mov bp, sp                                ; 89 e5                       ; 0xc2fa3
    mov si, dx                                ; 89 d6                       ; 0xc2fa5
    mov es, ax                                ; 8e c0                       ; 0xc2fa7 vgabios.c:2038
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2fa9
    mov word [es:si+002h], cx                 ; 26 89 4c 02                 ; 0xc2fac
    pop bp                                    ; 5d                          ; 0xc2fb0 vgabios.c:2039
    pop si                                    ; 5e                          ; 0xc2fb1
    retn                                      ; c3                          ; 0xc2fb2
  ; disGetNextSymbol 0xc2fb3 LB 0xb26 -> off=0x84 cb=00000000000003ca uValue=00000000000c3037 'int10_func'
    db  04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h, 005h
    db  004h, 003h, 002h, 001h, 000h, 0fah, 033h, 06ah, 030h, 0a8h, 030h, 0bch, 030h, 0cdh, 030h, 0e1h
    db  030h, 0f2h, 030h, 0fch, 030h, 036h, 031h, 03ah, 031h, 04bh, 031h, 068h, 031h, 085h, 031h, 0a4h
    db  031h, 0c1h, 031h, 0d8h, 031h, 0e4h, 031h, 0b4h, 032h, 0eeh, 032h, 021h, 033h, 036h, 033h, 073h
    db  033h, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h, 001h, 000h, 0fah
    db  033h, 003h, 032h, 029h, 032h, 03ah, 032h, 04bh, 032h, 003h, 032h, 029h, 032h, 03ah, 032h, 04bh
    db  032h, 05ch, 032h, 068h, 032h, 083h, 032h, 08bh, 032h, 093h, 032h, 09bh, 032h, 00ah, 009h, 006h
    db  004h, 002h, 001h, 000h, 0eeh, 033h, 09bh, 033h, 0a8h, 033h, 0b8h, 033h, 0c8h, 033h, 0ddh, 033h
    db  0eeh, 033h, 0eeh, 033h
int10_func:                                  ; 0xc3037 LB 0x3ca
    push bp                                   ; 55                          ; 0xc3037 vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc3038
    push si                                   ; 56                          ; 0xc303a
    push di                                   ; 57                          ; 0xc303b
    push ax                                   ; 50                          ; 0xc303c
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc303d
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3040 vgabios.c:2120
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3043
    cmp ax, strict word 0004fh                ; 3d 4f 00                    ; 0xc3046
    jnbe near 033fah                          ; 0f 87 ad 03                 ; 0xc3049
    push CS                                   ; 0e                          ; 0xc304d
    pop ES                                    ; 07                          ; 0xc304e
    mov cx, strict word 00016h                ; b9 16 00                    ; 0xc304f
    mov di, 02fb3h                            ; bf b3 2f                    ; 0xc3052
    repne scasb                               ; f2 ae                       ; 0xc3055
    sal cx, 1                                 ; d1 e1                       ; 0xc3057
    mov di, cx                                ; 89 cf                       ; 0xc3059
    mov bx, word [cs:di+02fc8h]               ; 2e 8b 9d c8 2f              ; 0xc305b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3060
    xor ah, ah                                ; 30 e4                       ; 0xc3063
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc3065
    jmp bx                                    ; ff e3                       ; 0xc3068
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc306a vgabios.c:2123
    xor ah, ah                                ; 30 e4                       ; 0xc306d
    call 00fdch                               ; e8 6a df                    ; 0xc306f
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3072 vgabios.c:2124
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3075
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc3078
    je short 03092h                           ; 74 15                       ; 0xc307b
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc307d
    je short 03089h                           ; 74 07                       ; 0xc3080
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc3082
    jbe short 03092h                          ; 76 0b                       ; 0xc3085
    jmp short 0309bh                          ; eb 12                       ; 0xc3087
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3089 vgabios.c:2126
    xor al, al                                ; 30 c0                       ; 0xc308c
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc308e
    jmp short 030a2h                          ; eb 10                       ; 0xc3090 vgabios.c:2127
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3092 vgabios.c:2135
    xor al, al                                ; 30 c0                       ; 0xc3095
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc3097
    jmp short 030a2h                          ; eb 07                       ; 0xc3099
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc309b vgabios.c:2138
    xor al, al                                ; 30 c0                       ; 0xc309e
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc30a0
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc30a2
    jmp near 033fah                           ; e9 52 03                    ; 0xc30a5 vgabios.c:2140
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc30a8 vgabios.c:2142
    movzx dx, al                              ; 0f b6 d0                    ; 0xc30ab
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc30ae
    shr ax, 008h                              ; c1 e8 08                    ; 0xc30b1
    xor ah, ah                                ; 30 e4                       ; 0xc30b4
    call 00dbah                               ; e8 01 dd                    ; 0xc30b6
    jmp near 033fah                           ; e9 3e 03                    ; 0xc30b9 vgabios.c:2143
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc30bc vgabios.c:2145
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc30bf
    shr ax, 008h                              ; c1 e8 08                    ; 0xc30c2
    xor ah, ah                                ; 30 e4                       ; 0xc30c5
    call 00e5eh                               ; e8 94 dd                    ; 0xc30c7
    jmp near 033fah                           ; e9 2d 03                    ; 0xc30ca vgabios.c:2146
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc30cd vgabios.c:2148
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc30d0
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc30d3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc30d6
    xor ah, ah                                ; 30 e4                       ; 0xc30d9
    call 00a88h                               ; e8 aa d9                    ; 0xc30db
    jmp near 033fah                           ; e9 19 03                    ; 0xc30de vgabios.c:2149
    xor al, al                                ; 30 c0                       ; 0xc30e1 vgabios.c:2155
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc30e3
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc30e6 vgabios.c:2156
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc30e9 vgabios.c:2157
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc30ec vgabios.c:2158
    jmp near 033fah                           ; e9 08 03                    ; 0xc30ef vgabios.c:2159
    mov al, dl                                ; 88 d0                       ; 0xc30f2 vgabios.c:2161
    xor ah, ah                                ; 30 e4                       ; 0xc30f4
    call 00f00h                               ; e8 07 de                    ; 0xc30f6
    jmp near 033fah                           ; e9 fe 02                    ; 0xc30f9 vgabios.c:2162
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc30fc vgabios.c:2164
    push ax                                   ; 50                          ; 0xc30ff
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3100
    push ax                                   ; 50                          ; 0xc3103
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3104
    xor ah, ah                                ; 30 e4                       ; 0xc3107
    push ax                                   ; 50                          ; 0xc3109
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc310a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc310d
    xor ah, ah                                ; 30 e4                       ; 0xc3110
    push ax                                   ; 50                          ; 0xc3112
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3113
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3116
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3119
    shr ax, 008h                              ; c1 e8 08                    ; 0xc311c
    movzx bx, al                              ; 0f b6 d8                    ; 0xc311f
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3122
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3125
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3128
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc312b
    xor ah, ah                                ; 30 e4                       ; 0xc312e
    call 0158bh                               ; e8 58 e4                    ; 0xc3130
    jmp near 033fah                           ; e9 c4 02                    ; 0xc3133 vgabios.c:2165
    xor al, al                                ; 30 c0                       ; 0xc3136 vgabios.c:2167
    jmp short 030ffh                          ; eb c5                       ; 0xc3138
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc313a vgabios.c:2170
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc313d
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3140
    xor ah, ah                                ; 30 e4                       ; 0xc3143
    call 00acbh                               ; e8 83 d9                    ; 0xc3145
    jmp near 033fah                           ; e9 af 02                    ; 0xc3148 vgabios.c:2171
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc314b vgabios.c:2173
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc314e
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3151
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3154
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3157
    movzx dx, al                              ; 0f b6 d0                    ; 0xc315a
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc315d
    xor ah, ah                                ; 30 e4                       ; 0xc3160
    call 01d2bh                               ; e8 c6 eb                    ; 0xc3162
    jmp near 033fah                           ; e9 92 02                    ; 0xc3165 vgabios.c:2174
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3168 vgabios.c:2176
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc316b
    movzx bx, al                              ; 0f b6 d8                    ; 0xc316e
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3171
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3174
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3177
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc317a
    xor ah, ah                                ; 30 e4                       ; 0xc317d
    call 01e93h                               ; e8 11 ed                    ; 0xc317f
    jmp near 033fah                           ; e9 75 02                    ; 0xc3182 vgabios.c:2177
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc3185 vgabios.c:2179
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3188
    mov al, dl                                ; 88 d0                       ; 0xc318b
    movzx dx, al                              ; 0f b6 d0                    ; 0xc318d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3190
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3193
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3196
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3199
    xor ah, ah                                ; 30 e4                       ; 0xc319c
    call 02002h                               ; e8 61 ee                    ; 0xc319e
    jmp near 033fah                           ; e9 56 02                    ; 0xc31a1 vgabios.c:2180
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc31a4 vgabios.c:2182
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc31a7
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc31aa
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc31ad
    shr ax, 008h                              ; c1 e8 08                    ; 0xc31b0
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc31b3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc31b6
    xor ah, ah                                ; 30 e4                       ; 0xc31b9
    call 00bf5h                               ; e8 37 da                    ; 0xc31bb
    jmp near 033fah                           ; e9 39 02                    ; 0xc31be vgabios.c:2183
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc31c1 vgabios.c:2191
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc31c4
    movzx bx, al                              ; 0f b6 d8                    ; 0xc31c7
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc31ca
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc31cd
    xor ah, ah                                ; 30 e4                       ; 0xc31d0
    call 0216ch                               ; e8 97 ef                    ; 0xc31d2
    jmp near 033fah                           ; e9 22 02                    ; 0xc31d5 vgabios.c:2192
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc31d8 vgabios.c:2195
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc31db
    call 00d2eh                               ; e8 4d db                    ; 0xc31de
    jmp near 033fah                           ; e9 16 02                    ; 0xc31e1 vgabios.c:2196
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc31e4 vgabios.c:2198
    jnbe near 033fah                          ; 0f 87 0f 02                 ; 0xc31e7
    push CS                                   ; 0e                          ; 0xc31eb
    pop ES                                    ; 07                          ; 0xc31ec
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc31ed
    mov di, 02ff4h                            ; bf f4 2f                    ; 0xc31f0
    repne scasb                               ; f2 ae                       ; 0xc31f3
    sal cx, 1                                 ; d1 e1                       ; 0xc31f5
    mov di, cx                                ; 89 cf                       ; 0xc31f7
    mov dx, word [cs:di+03002h]               ; 2e 8b 95 02 30              ; 0xc31f9
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc31fe
    jmp dx                                    ; ff e2                       ; 0xc3201
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3203 vgabios.c:2202
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3206
    xor ah, ah                                ; 30 e4                       ; 0xc3209
    push ax                                   ; 50                          ; 0xc320b
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc320c
    xor ah, ah                                ; 30 e4                       ; 0xc320f
    push ax                                   ; 50                          ; 0xc3211
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3212
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3215
    xor ah, ah                                ; 30 e4                       ; 0xc3218
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc321a
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc321d
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3220
    call 024d4h                               ; e8 ae f2                    ; 0xc3223
    jmp near 033fah                           ; e9 d1 01                    ; 0xc3226 vgabios.c:2203
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3229 vgabios.c:2206
    movzx dx, al                              ; 0f b6 d0                    ; 0xc322c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc322f
    xor ah, ah                                ; 30 e4                       ; 0xc3232
    call 02551h                               ; e8 1a f3                    ; 0xc3234
    jmp near 033fah                           ; e9 c0 01                    ; 0xc3237 vgabios.c:2207
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc323a vgabios.c:2210
    movzx dx, al                              ; 0f b6 d0                    ; 0xc323d
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3240
    xor ah, ah                                ; 30 e4                       ; 0xc3243
    call 025c1h                               ; e8 79 f3                    ; 0xc3245
    jmp near 033fah                           ; e9 af 01                    ; 0xc3248 vgabios.c:2211
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc324b vgabios.c:2214
    movzx dx, al                              ; 0f b6 d0                    ; 0xc324e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3251
    xor ah, ah                                ; 30 e4                       ; 0xc3254
    call 02633h                               ; e8 da f3                    ; 0xc3256
    jmp near 033fah                           ; e9 9e 01                    ; 0xc3259 vgabios.c:2215
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc325c vgabios.c:2217
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc325f
    call 026a5h                               ; e8 40 f4                    ; 0xc3262
    jmp near 033fah                           ; e9 92 01                    ; 0xc3265 vgabios.c:2218
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3268 vgabios.c:2220
    xor ah, ah                                ; 30 e4                       ; 0xc326b
    push ax                                   ; 50                          ; 0xc326d
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc326e
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3271
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3274
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3277
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc327a
    call 026aah                               ; e8 2a f4                    ; 0xc327d
    jmp near 033fah                           ; e9 77 01                    ; 0xc3280 vgabios.c:2221
    xor ah, ah                                ; 30 e4                       ; 0xc3283 vgabios.c:2223
    call 026b1h                               ; e8 29 f4                    ; 0xc3285
    jmp near 033fah                           ; e9 6f 01                    ; 0xc3288 vgabios.c:2224
    xor ah, ah                                ; 30 e4                       ; 0xc328b vgabios.c:2226
    call 026b6h                               ; e8 26 f4                    ; 0xc328d
    jmp near 033fah                           ; e9 67 01                    ; 0xc3290 vgabios.c:2227
    xor ah, ah                                ; 30 e4                       ; 0xc3293 vgabios.c:2229
    call 026bbh                               ; e8 23 f4                    ; 0xc3295
    jmp near 033fah                           ; e9 5f 01                    ; 0xc3298 vgabios.c:2230
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc329b vgabios.c:2232
    push ax                                   ; 50                          ; 0xc329e
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc329f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc32a2
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc32a5
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc32a8
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc32ab
    call 00b73h                               ; e8 c2 d8                    ; 0xc32ae
    jmp near 033fah                           ; e9 46 01                    ; 0xc32b1 vgabios.c:2240
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc32b4 vgabios.c:2242
    xor ah, ah                                ; 30 e4                       ; 0xc32b7
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc32b9
    je short 032e6h                           ; 74 28                       ; 0xc32bc
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc32be
    je short 032d0h                           ; 74 0d                       ; 0xc32c1
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc32c3
    jne near 033fah                           ; 0f 85 30 01                 ; 0xc32c6
    call 026c0h                               ; e8 f3 f3                    ; 0xc32ca vgabios.c:2245
    jmp near 033fah                           ; e9 2a 01                    ; 0xc32cd vgabios.c:2246
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc32d0 vgabios.c:2248
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc32d3
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc32d6
    call 026c5h                               ; e8 e9 f3                    ; 0xc32d9
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32dc vgabios.c:2249
    xor al, al                                ; 30 c0                       ; 0xc32df
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc32e1
    jmp near 030a2h                           ; e9 bc fd                    ; 0xc32e3
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc32e6 vgabios.c:2252
    call 026cah                               ; e8 de f3                    ; 0xc32e9
    jmp short 032dch                          ; eb ee                       ; 0xc32ec
    push word [bp+008h]                       ; ff 76 08                    ; 0xc32ee vgabios.c:2262
    push word [bp+016h]                       ; ff 76 16                    ; 0xc32f1
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc32f4
    xor ah, ah                                ; 30 e4                       ; 0xc32f7
    push ax                                   ; 50                          ; 0xc32f9
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc32fa
    shr ax, 008h                              ; c1 e8 08                    ; 0xc32fd
    xor ah, ah                                ; 30 e4                       ; 0xc3300
    push ax                                   ; 50                          ; 0xc3302
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3303
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3306
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3309
    shr ax, 008h                              ; c1 e8 08                    ; 0xc330c
    xor ah, ah                                ; 30 e4                       ; 0xc330f
    movzx si, dl                              ; 0f b6 f2                    ; 0xc3311
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3314
    mov dx, ax                                ; 89 c2                       ; 0xc3317
    mov ax, si                                ; 89 f0                       ; 0xc3319
    call 026cfh                               ; e8 b1 f3                    ; 0xc331b
    jmp near 033fah                           ; e9 d9 00                    ; 0xc331e vgabios.c:2263
    mov bx, si                                ; 89 f3                       ; 0xc3321 vgabios.c:2265
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3323
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3326
    call 0276bh                               ; e8 3f f4                    ; 0xc3329
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc332c vgabios.c:2266
    xor al, al                                ; 30 c0                       ; 0xc332f
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3331
    jmp near 030a2h                           ; e9 6c fd                    ; 0xc3333
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3336 vgabios.c:2269
    je short 0335dh                           ; 74 22                       ; 0xc3339
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc333b
    je short 0334fh                           ; 74 0f                       ; 0xc333e
    test ax, ax                               ; 85 c0                       ; 0xc3340
    jne short 03369h                          ; 75 25                       ; 0xc3342
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3344 vgabios.c:2272
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3347
    call 0288fh                               ; e8 42 f5                    ; 0xc334a
    jmp short 03369h                          ; eb 1a                       ; 0xc334d vgabios.c:2273
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc334f vgabios.c:2275
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3352
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3355
    call 028a1h                               ; e8 46 f5                    ; 0xc3358
    jmp short 03369h                          ; eb 0c                       ; 0xc335b vgabios.c:2276
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc335d vgabios.c:2278
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3360
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3363
    call 02c0ah                               ; e8 a1 f8                    ; 0xc3366
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3369 vgabios.c:2285
    xor al, al                                ; 30 c0                       ; 0xc336c
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc336e
    jmp near 030a2h                           ; e9 2f fd                    ; 0xc3370
    call 007bfh                               ; e8 49 d4                    ; 0xc3373 vgabios.c:2290
    test ax, ax                               ; 85 c0                       ; 0xc3376
    je near 033f5h                            ; 0f 84 79 00                 ; 0xc3378
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc337c vgabios.c:2291
    xor ah, ah                                ; 30 e4                       ; 0xc337f
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3381
    jnbe short 033eeh                         ; 77 68                       ; 0xc3384
    push CS                                   ; 0e                          ; 0xc3386
    pop ES                                    ; 07                          ; 0xc3387
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3388
    mov di, 03020h                            ; bf 20 30                    ; 0xc338b
    repne scasb                               ; f2 ae                       ; 0xc338e
    sal cx, 1                                 ; d1 e1                       ; 0xc3390
    mov di, cx                                ; 89 cf                       ; 0xc3392
    mov ax, word [cs:di+03027h]               ; 2e 8b 85 27 30              ; 0xc3394
    jmp ax                                    ; ff e0                       ; 0xc3399
    mov bx, si                                ; 89 f3                       ; 0xc339b vgabios.c:2294
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc339d
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc33a0
    call 035b6h                               ; e8 10 02                    ; 0xc33a3
    jmp short 033fah                          ; eb 52                       ; 0xc33a6 vgabios.c:2295
    mov cx, si                                ; 89 f1                       ; 0xc33a8 vgabios.c:2297
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc33aa
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33ad
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc33b0
    call 036dfh                               ; e8 29 03                    ; 0xc33b3
    jmp short 033fah                          ; eb 42                       ; 0xc33b6 vgabios.c:2298
    mov cx, si                                ; 89 f1                       ; 0xc33b8 vgabios.c:2300
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc33ba
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc33bd
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc33c0
    call 03797h                               ; e8 d1 03                    ; 0xc33c3
    jmp short 033fah                          ; eb 32                       ; 0xc33c6 vgabios.c:2301
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc33c8 vgabios.c:2303
    push ax                                   ; 50                          ; 0xc33cb
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc33cc
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc33cf
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33d2
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc33d5
    call 0397eh                               ; e8 a3 05                    ; 0xc33d8
    jmp short 033fah                          ; eb 1d                       ; 0xc33db vgabios.c:2304
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc33dd vgabios.c:2306
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc33e0
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc33e3
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc33e6
    call 03a0ah                               ; e8 1e 06                    ; 0xc33e9
    jmp short 033fah                          ; eb 0c                       ; 0xc33ec vgabios.c:2307
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc33ee vgabios.c:2329
    jmp short 033fah                          ; eb 05                       ; 0xc33f3 vgabios.c:2332
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc33f5 vgabios.c:2334
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc33fa vgabios.c:2344
    pop di                                    ; 5f                          ; 0xc33fd
    pop si                                    ; 5e                          ; 0xc33fe
    pop bp                                    ; 5d                          ; 0xc33ff
    retn                                      ; c3                          ; 0xc3400
  ; disGetNextSymbol 0xc3401 LB 0x6d8 -> off=0x0 cb=000000000000001f uValue=00000000000c3401 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3401 LB 0x1f
    push bp                                   ; 55                          ; 0xc3401 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3402
    push bx                                   ; 53                          ; 0xc3404
    push dx                                   ; 52                          ; 0xc3405
    mov bx, ax                                ; 89 c3                       ; 0xc3406
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3408 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc340b
    call 00570h                               ; e8 5f d1                    ; 0xc340e
    mov ax, bx                                ; 89 d8                       ; 0xc3411 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3413
    call 00570h                               ; e8 57 d1                    ; 0xc3416
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3419 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc341c
    pop bx                                    ; 5b                          ; 0xc341d
    pop bp                                    ; 5d                          ; 0xc341e
    retn                                      ; c3                          ; 0xc341f
  ; disGetNextSymbol 0xc3420 LB 0x6b9 -> off=0x0 cb=000000000000001f uValue=00000000000c3420 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3420 LB 0x1f
    push bp                                   ; 55                          ; 0xc3420 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3421
    push bx                                   ; 53                          ; 0xc3423
    push dx                                   ; 52                          ; 0xc3424
    mov bx, ax                                ; 89 c3                       ; 0xc3425
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3427 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc342a
    call 00570h                               ; e8 40 d1                    ; 0xc342d
    mov ax, bx                                ; 89 d8                       ; 0xc3430 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3432
    call 00570h                               ; e8 38 d1                    ; 0xc3435
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3438 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc343b
    pop bx                                    ; 5b                          ; 0xc343c
    pop bp                                    ; 5d                          ; 0xc343d
    retn                                      ; c3                          ; 0xc343e
  ; disGetNextSymbol 0xc343f LB 0x69a -> off=0x0 cb=0000000000000019 uValue=00000000000c343f 'dispi_get_yres'
dispi_get_yres:                              ; 0xc343f LB 0x19
    push bp                                   ; 55                          ; 0xc343f vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3440
    push dx                                   ; 52                          ; 0xc3442
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3443 vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3446
    call 00570h                               ; e8 24 d1                    ; 0xc3449
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc344c vbe.c:121
    call 00577h                               ; e8 25 d1                    ; 0xc344f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3452 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3455
    pop bp                                    ; 5d                          ; 0xc3456
    retn                                      ; c3                          ; 0xc3457
  ; disGetNextSymbol 0xc3458 LB 0x681 -> off=0x0 cb=000000000000001f uValue=00000000000c3458 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3458 LB 0x1f
    push bp                                   ; 55                          ; 0xc3458 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3459
    push bx                                   ; 53                          ; 0xc345b
    push dx                                   ; 52                          ; 0xc345c
    mov bx, ax                                ; 89 c3                       ; 0xc345d
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc345f vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3462
    call 00570h                               ; e8 08 d1                    ; 0xc3465
    mov ax, bx                                ; 89 d8                       ; 0xc3468 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc346a
    call 00570h                               ; e8 00 d1                    ; 0xc346d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3470 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3473
    pop bx                                    ; 5b                          ; 0xc3474
    pop bp                                    ; 5d                          ; 0xc3475
    retn                                      ; c3                          ; 0xc3476
  ; disGetNextSymbol 0xc3477 LB 0x662 -> off=0x0 cb=0000000000000019 uValue=00000000000c3477 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3477 LB 0x19
    push bp                                   ; 55                          ; 0xc3477 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3478
    push dx                                   ; 52                          ; 0xc347a
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc347b vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc347e
    call 00570h                               ; e8 ec d0                    ; 0xc3481
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3484 vbe.c:136
    call 00577h                               ; e8 ed d0                    ; 0xc3487
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc348a vbe.c:137
    pop dx                                    ; 5a                          ; 0xc348d
    pop bp                                    ; 5d                          ; 0xc348e
    retn                                      ; c3                          ; 0xc348f
  ; disGetNextSymbol 0xc3490 LB 0x649 -> off=0x0 cb=000000000000001f uValue=00000000000c3490 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3490 LB 0x1f
    push bp                                   ; 55                          ; 0xc3490 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3491
    push bx                                   ; 53                          ; 0xc3493
    push dx                                   ; 52                          ; 0xc3494
    mov bx, ax                                ; 89 c3                       ; 0xc3495
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3497 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc349a
    call 00570h                               ; e8 d0 d0                    ; 0xc349d
    mov ax, bx                                ; 89 d8                       ; 0xc34a0 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc34a2
    call 00570h                               ; e8 c8 d0                    ; 0xc34a5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc34a8 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc34ab
    pop bx                                    ; 5b                          ; 0xc34ac
    pop bp                                    ; 5d                          ; 0xc34ad
    retn                                      ; c3                          ; 0xc34ae
  ; disGetNextSymbol 0xc34af LB 0x62a -> off=0x0 cb=0000000000000019 uValue=00000000000c34af 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc34af LB 0x19
    push bp                                   ; 55                          ; 0xc34af vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc34b0
    push dx                                   ; 52                          ; 0xc34b2
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc34b3 vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc34b6
    call 00570h                               ; e8 b4 d0                    ; 0xc34b9
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc34bc vbe.c:151
    call 00577h                               ; e8 b5 d0                    ; 0xc34bf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc34c2 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc34c5
    pop bp                                    ; 5d                          ; 0xc34c6
    retn                                      ; c3                          ; 0xc34c7
  ; disGetNextSymbol 0xc34c8 LB 0x611 -> off=0x0 cb=0000000000000019 uValue=00000000000c34c8 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc34c8 LB 0x19
    push bp                                   ; 55                          ; 0xc34c8 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc34c9
    push dx                                   ; 52                          ; 0xc34cb
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc34cc vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc34cf
    call 00570h                               ; e8 9b d0                    ; 0xc34d2
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc34d5 vbe.c:157
    call 00577h                               ; e8 9c d0                    ; 0xc34d8
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc34db vbe.c:158
    pop dx                                    ; 5a                          ; 0xc34de
    pop bp                                    ; 5d                          ; 0xc34df
    retn                                      ; c3                          ; 0xc34e0
  ; disGetNextSymbol 0xc34e1 LB 0x5f8 -> off=0x0 cb=0000000000000012 uValue=00000000000c34e1 'in_word'
in_word:                                     ; 0xc34e1 LB 0x12
    push bp                                   ; 55                          ; 0xc34e1 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc34e2
    push bx                                   ; 53                          ; 0xc34e4
    mov bx, ax                                ; 89 c3                       ; 0xc34e5
    mov ax, dx                                ; 89 d0                       ; 0xc34e7
    mov dx, bx                                ; 89 da                       ; 0xc34e9 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc34eb
    in ax, DX                                 ; ed                          ; 0xc34ec vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc34ed vbe.c:164
    pop bx                                    ; 5b                          ; 0xc34f0
    pop bp                                    ; 5d                          ; 0xc34f1
    retn                                      ; c3                          ; 0xc34f2
  ; disGetNextSymbol 0xc34f3 LB 0x5e6 -> off=0x0 cb=0000000000000014 uValue=00000000000c34f3 'in_byte'
in_byte:                                     ; 0xc34f3 LB 0x14
    push bp                                   ; 55                          ; 0xc34f3 vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc34f4
    push bx                                   ; 53                          ; 0xc34f6
    mov bx, ax                                ; 89 c3                       ; 0xc34f7
    mov ax, dx                                ; 89 d0                       ; 0xc34f9
    mov dx, bx                                ; 89 da                       ; 0xc34fb vbe.c:168
    out DX, ax                                ; ef                          ; 0xc34fd
    in AL, DX                                 ; ec                          ; 0xc34fe vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc34ff
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3501 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3504
    pop bp                                    ; 5d                          ; 0xc3505
    retn                                      ; c3                          ; 0xc3506
  ; disGetNextSymbol 0xc3507 LB 0x5d2 -> off=0x0 cb=0000000000000014 uValue=00000000000c3507 'dispi_get_id'
dispi_get_id:                                ; 0xc3507 LB 0x14
    push bp                                   ; 55                          ; 0xc3507 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3508
    push dx                                   ; 52                          ; 0xc350a
    xor ax, ax                                ; 31 c0                       ; 0xc350b vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc350d
    out DX, ax                                ; ef                          ; 0xc3510
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3511 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3514
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3515 vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3518
    pop bp                                    ; 5d                          ; 0xc3519
    retn                                      ; c3                          ; 0xc351a
  ; disGetNextSymbol 0xc351b LB 0x5be -> off=0x0 cb=000000000000001a uValue=00000000000c351b 'dispi_set_id'
dispi_set_id:                                ; 0xc351b LB 0x1a
    push bp                                   ; 55                          ; 0xc351b vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc351c
    push bx                                   ; 53                          ; 0xc351e
    push dx                                   ; 52                          ; 0xc351f
    mov bx, ax                                ; 89 c3                       ; 0xc3520
    xor ax, ax                                ; 31 c0                       ; 0xc3522 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3524
    out DX, ax                                ; ef                          ; 0xc3527
    mov ax, bx                                ; 89 d8                       ; 0xc3528 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc352a
    out DX, ax                                ; ef                          ; 0xc352d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc352e vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3531
    pop bx                                    ; 5b                          ; 0xc3532
    pop bp                                    ; 5d                          ; 0xc3533
    retn                                      ; c3                          ; 0xc3534
  ; disGetNextSymbol 0xc3535 LB 0x5a4 -> off=0x0 cb=000000000000002c uValue=00000000000c3535 'vbe_init'
vbe_init:                                    ; 0xc3535 LB 0x2c
    push bp                                   ; 55                          ; 0xc3535 vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3536
    push bx                                   ; 53                          ; 0xc3538
    push dx                                   ; 52                          ; 0xc3539
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc353a vbe.c:190
    call 0351bh                               ; e8 db ff                    ; 0xc353d
    call 03507h                               ; e8 c4 ff                    ; 0xc3540 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3543
    jne short 0355ah                          ; 75 12                       ; 0xc3546
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc3548 vbe.c:193
    mov dx, 000b9h                            ; ba b9 00                    ; 0xc354b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc354e
    call 02f65h                               ; e8 11 fa                    ; 0xc3551
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3554 vbe.c:194
    call 0351bh                               ; e8 c1 ff                    ; 0xc3557
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc355a vbe.c:199
    pop dx                                    ; 5a                          ; 0xc355d
    pop bx                                    ; 5b                          ; 0xc355e
    pop bp                                    ; 5d                          ; 0xc355f
    retn                                      ; c3                          ; 0xc3560
  ; disGetNextSymbol 0xc3561 LB 0x578 -> off=0x0 cb=0000000000000055 uValue=00000000000c3561 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3561 LB 0x55
    push bp                                   ; 55                          ; 0xc3561 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3562
    push bx                                   ; 53                          ; 0xc3564
    push cx                                   ; 51                          ; 0xc3565
    push si                                   ; 56                          ; 0xc3566
    push di                                   ; 57                          ; 0xc3567
    mov di, ax                                ; 89 c7                       ; 0xc3568
    mov si, dx                                ; 89 d6                       ; 0xc356a
    xor dx, dx                                ; 31 d2                       ; 0xc356c vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc356e
    call 034e1h                               ; e8 6d ff                    ; 0xc3571
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3574 vbe.c:209
    jne short 035abh                          ; 75 32                       ; 0xc3577
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3579 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc357c vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc357e
    call 034e1h                               ; e8 5d ff                    ; 0xc3581
    mov cx, ax                                ; 89 c1                       ; 0xc3584
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3586 vbe.c:219
    je short 035abh                           ; 74 20                       ; 0xc3589
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc358b vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc358e
    call 034e1h                               ; e8 4d ff                    ; 0xc3591
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3594
    cmp cx, di                                ; 39 f9                       ; 0xc3597 vbe.c:223
    jne short 035a7h                          ; 75 0c                       ; 0xc3599
    test si, si                               ; 85 f6                       ; 0xc359b vbe.c:225
    jne short 035a3h                          ; 75 04                       ; 0xc359d
    mov ax, bx                                ; 89 d8                       ; 0xc359f vbe.c:226
    jmp short 035adh                          ; eb 0a                       ; 0xc35a1
    test AL, strict byte 080h                 ; a8 80                       ; 0xc35a3 vbe.c:227
    jne short 0359fh                          ; 75 f8                       ; 0xc35a5
    mov bx, dx                                ; 89 d3                       ; 0xc35a7 vbe.c:230
    jmp short 0357eh                          ; eb d3                       ; 0xc35a9 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc35ab vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc35ad vbe.c:239
    pop di                                    ; 5f                          ; 0xc35b0
    pop si                                    ; 5e                          ; 0xc35b1
    pop cx                                    ; 59                          ; 0xc35b2
    pop bx                                    ; 5b                          ; 0xc35b3
    pop bp                                    ; 5d                          ; 0xc35b4
    retn                                      ; c3                          ; 0xc35b5
  ; disGetNextSymbol 0xc35b6 LB 0x523 -> off=0x0 cb=0000000000000129 uValue=00000000000c35b6 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc35b6 LB 0x129
    push bp                                   ; 55                          ; 0xc35b6 vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc35b7
    push cx                                   ; 51                          ; 0xc35b9
    push si                                   ; 56                          ; 0xc35ba
    push di                                   ; 57                          ; 0xc35bb
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc35bc
    mov si, ax                                ; 89 c6                       ; 0xc35bf
    mov di, dx                                ; 89 d7                       ; 0xc35c1
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc35c3
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc35c6 vbe.c:275
    call 005b7h                               ; e8 e9 cf                    ; 0xc35cb vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc35ce
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc35d1 vbe.c:281
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc35d4
    xor dx, dx                                ; 31 d2                       ; 0xc35d7 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc35d9
    call 034e1h                               ; e8 02 ff                    ; 0xc35dc
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc35df vbe.c:285
    je short 035eeh                           ; 74 0a                       ; 0xc35e2
    push SS                                   ; 16                          ; 0xc35e4 vbe.c:287
    pop ES                                    ; 07                          ; 0xc35e5
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc35e6
    jmp near 036d7h                           ; e9 e9 00                    ; 0xc35eb vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc35ee vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc35f1 vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc35f6 vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc35f9
    jne short 03608h                          ; 75 07                       ; 0xc35ff
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3601
    je short 03617h                           ; 74 0f                       ; 0xc3606
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3608
    jne short 0361ch                          ; 75 0c                       ; 0xc360e
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3610
    jne short 0361ch                          ; 75 05                       ; 0xc3615
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3617 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc361c vbe.c:318
    db  066h, 026h, 0c7h, 007h, 056h, 045h, 053h, 041h
    ; mov dword [es:bx], strict dword 041534556h ; 66 26 c7 07 56 45 53 41  ; 0xc361f
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3627 vbe.c:324
    mov word [es:bx+006h], 07c6ch             ; 26 c7 47 06 6c 7c           ; 0xc362d vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3633
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 000h
    ; mov dword [es:bx+00ah], strict dword 000000001h ; 66 26 c7 47 0a 01 00 00 00; 0xc3637 vbe.c:330
    mov word [es:bx+010h], di                 ; 26 89 7f 10                 ; 0xc3640 vbe.c:336
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3644 vbe.c:337
    add ax, strict word 00022h                ; 05 22 00                    ; 0xc3647
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc364a
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc364e vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3651
    call 034e1h                               ; e8 8a fe                    ; 0xc3654
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3657
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc365a
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc365e vbe.c:342
    je short 03688h                           ; 74 24                       ; 0xc3662
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3664 vbe.c:345
    mov word [es:bx+016h], 07c81h             ; 26 c7 47 16 81 7c           ; 0xc366a vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3670
    mov word [es:bx+01ah], 07c94h             ; 26 c7 47 1a 94 7c           ; 0xc3674 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc367a
    mov word [es:bx+01eh], 07cb5h             ; 26 c7 47 1e b5 7c           ; 0xc367e vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3684
    mov dx, cx                                ; 89 ca                       ; 0xc3688 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc368a
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc368d
    call 034f3h                               ; e8 60 fe                    ; 0xc3690
    xor ah, ah                                ; 30 e4                       ; 0xc3693 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3695
    jnbe short 036b3h                         ; 77 19                       ; 0xc3698
    mov dx, cx                                ; 89 ca                       ; 0xc369a vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc369c
    call 034e1h                               ; e8 3f fe                    ; 0xc369f
    mov bx, ax                                ; 89 c3                       ; 0xc36a2
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc36a4 vbe.c:362
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc36a7
    mov ax, di                                ; 89 f8                       ; 0xc36aa
    call 02f81h                               ; e8 d2 f8                    ; 0xc36ac
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc36af vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc36b3 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc36b6 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc36b8
    call 034e1h                               ; e8 23 fe                    ; 0xc36bb
    mov bx, ax                                ; 89 c3                       ; 0xc36be
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc36c0 vbe.c:368
    jne short 03688h                          ; 75 c3                       ; 0xc36c3
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc36c5 vbe.c:371
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc36c8
    mov ax, di                                ; 89 f8                       ; 0xc36cb
    call 02f81h                               ; e8 b1 f8                    ; 0xc36cd
    push SS                                   ; 16                          ; 0xc36d0 vbe.c:372
    pop ES                                    ; 07                          ; 0xc36d1
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc36d2
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc36d7 vbe.c:373
    pop di                                    ; 5f                          ; 0xc36da
    pop si                                    ; 5e                          ; 0xc36db
    pop cx                                    ; 59                          ; 0xc36dc
    pop bp                                    ; 5d                          ; 0xc36dd
    retn                                      ; c3                          ; 0xc36de
  ; disGetNextSymbol 0xc36df LB 0x3fa -> off=0x0 cb=00000000000000b8 uValue=00000000000c36df 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc36df LB 0xb8
    push bp                                   ; 55                          ; 0xc36df vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc36e0
    push si                                   ; 56                          ; 0xc36e2
    push di                                   ; 57                          ; 0xc36e3
    push ax                                   ; 50                          ; 0xc36e4
    push ax                                   ; 50                          ; 0xc36e5
    push ax                                   ; 50                          ; 0xc36e6
    mov ax, dx                                ; 89 d0                       ; 0xc36e7
    mov si, bx                                ; 89 de                       ; 0xc36e9
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc36eb
    test dh, 040h                             ; f6 c6 40                    ; 0xc36ee vbe.c:396
    db  00fh, 095h, 0c2h
    ; setne dl                                  ; 0f 95 c2                  ; 0xc36f1
    xor dh, dh                                ; 30 f6                       ; 0xc36f4
    and ah, 001h                              ; 80 e4 01                    ; 0xc36f6 vbe.c:397
    call 03561h                               ; e8 65 fe                    ; 0xc36f9 vbe.c:399
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc36fc
    test ax, ax                               ; 85 c0                       ; 0xc36ff vbe.c:401
    je near 03785h                            ; 0f 84 80 00                 ; 0xc3701
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3705 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3708
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc370a
    mov es, bx                                ; 8e c3                       ; 0xc370d
    cld                                       ; fc                          ; 0xc370f
    jcxz 03714h                               ; e3 02                       ; 0xc3710
    rep stosb                                 ; f3 aa                       ; 0xc3712
    xor cx, cx                                ; 31 c9                       ; 0xc3714 vbe.c:407
    jmp short 0371dh                          ; eb 05                       ; 0xc3716
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3718
    jnc short 0373ah                          ; 73 1d                       ; 0xc371b
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc371d vbe.c:410
    inc dx                                    ; 42                          ; 0xc3720
    inc dx                                    ; 42                          ; 0xc3721
    add dx, cx                                ; 01 ca                       ; 0xc3722
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3724
    call 034f3h                               ; e8 c9 fd                    ; 0xc3727
    movzx bx, al                              ; 0f b6 d8                    ; 0xc372a vbe.c:411
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc372d
    add dx, cx                                ; 01 ca                       ; 0xc3730
    mov ax, si                                ; 89 f0                       ; 0xc3732
    call 02f65h                               ; e8 2e f8                    ; 0xc3734
    inc cx                                    ; 41                          ; 0xc3737 vbe.c:412
    jmp short 03718h                          ; eb de                       ; 0xc3738
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc373a vbe.c:413
    inc dx                                    ; 42                          ; 0xc373d
    inc dx                                    ; 42                          ; 0xc373e
    mov ax, si                                ; 89 f0                       ; 0xc373f
    call 02f57h                               ; e8 13 f8                    ; 0xc3741
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3744 vbe.c:414
    je short 03764h                           ; 74 1c                       ; 0xc3746
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3748 vbe.c:415
    add dx, strict byte 0000ch                ; 83 c2 0c                    ; 0xc374b
    mov bx, 00629h                            ; bb 29 06                    ; 0xc374e
    mov ax, si                                ; 89 f0                       ; 0xc3751
    call 02f81h                               ; e8 2b f8                    ; 0xc3753
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3756 vbe.c:417
    add dx, strict byte 0000eh                ; 83 c2 0e                    ; 0xc3759
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc375c
    mov ax, si                                ; 89 f0                       ; 0xc375f
    call 02f81h                               ; e8 1d f8                    ; 0xc3761
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3764 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3767
    call 00570h                               ; e8 03 ce                    ; 0xc376a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc376d vbe.c:421
    call 00577h                               ; e8 04 ce                    ; 0xc3770
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3773
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc3776
    mov bx, ax                                ; 89 c3                       ; 0xc3779
    mov ax, si                                ; 89 f0                       ; 0xc377b
    call 02f81h                               ; e8 01 f8                    ; 0xc377d
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3780 vbe.c:423
    jmp short 03788h                          ; eb 03                       ; 0xc3783 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3785 vbe.c:428
    push SS                                   ; 16                          ; 0xc3788 vbe.c:431
    pop ES                                    ; 07                          ; 0xc3789
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc378a
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc378d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3790 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3793
    pop si                                    ; 5e                          ; 0xc3794
    pop bp                                    ; 5d                          ; 0xc3795
    retn                                      ; c3                          ; 0xc3796
  ; disGetNextSymbol 0xc3797 LB 0x342 -> off=0x0 cb=00000000000000e9 uValue=00000000000c3797 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3797 LB 0xe9
    push bp                                   ; 55                          ; 0xc3797 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3798
    push si                                   ; 56                          ; 0xc379a
    push di                                   ; 57                          ; 0xc379b
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc379c
    mov si, ax                                ; 89 c6                       ; 0xc379f
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc37a1
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc37a4 vbe.c:452
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0                  ; 0xc37a8
    movzx dx, al                              ; 0f b6 d0                    ; 0xc37ab
    mov ax, dx                                ; 89 d0                       ; 0xc37ae
    test dx, dx                               ; 85 d2                       ; 0xc37b0 vbe.c:453
    je short 037b7h                           ; 74 03                       ; 0xc37b2
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc37b4
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc37b7
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc37ba vbe.c:454
    je short 037c5h                           ; 74 05                       ; 0xc37be
    mov dx, 00080h                            ; ba 80 00                    ; 0xc37c0
    jmp short 037c7h                          ; eb 02                       ; 0xc37c3
    xor dx, dx                                ; 31 d2                       ; 0xc37c5
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc37c7
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc37ca vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc37ce vbe.c:459
    jnc short 037e7h                          ; 73 12                       ; 0xc37d3
    xor ax, ax                                ; 31 c0                       ; 0xc37d5 vbe.c:463
    call 005ddh                               ; e8 03 ce                    ; 0xc37d7
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc37da vbe.c:467
    call 00fdch                               ; e8 fb d7                    ; 0xc37de
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc37e1 vbe.c:468
    jmp near 03876h                           ; e9 8f 00                    ; 0xc37e4 vbe.c:469
    mov dx, ax                                ; 89 c2                       ; 0xc37e7 vbe.c:472
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc37e9
    call 03561h                               ; e8 72 fd                    ; 0xc37ec
    mov bx, ax                                ; 89 c3                       ; 0xc37ef
    test ax, ax                               ; 85 c0                       ; 0xc37f1 vbe.c:474
    je near 03873h                            ; 0f 84 7c 00                 ; 0xc37f3
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc37f7 vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc37fa
    call 034e1h                               ; e8 e1 fc                    ; 0xc37fd
    mov cx, ax                                ; 89 c1                       ; 0xc3800
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3802 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3805
    call 034e1h                               ; e8 d6 fc                    ; 0xc3808
    mov di, ax                                ; 89 c7                       ; 0xc380b
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc380d vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3810
    call 034f3h                               ; e8 dd fc                    ; 0xc3813
    mov bl, al                                ; 88 c3                       ; 0xc3816
    mov dl, al                                ; 88 c2                       ; 0xc3818
    xor ax, ax                                ; 31 c0                       ; 0xc381a vbe.c:489
    call 005ddh                               ; e8 be cd                    ; 0xc381c
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc381f vbe.c:491
    jne short 0382ah                          ; 75 06                       ; 0xc3822
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3824 vbe.c:493
    call 00fdch                               ; e8 b2 d7                    ; 0xc3827
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc382a vbe.c:496
    call 03458h                               ; e8 28 fc                    ; 0xc382d
    mov ax, cx                                ; 89 c8                       ; 0xc3830 vbe.c:497
    call 03401h                               ; e8 cc fb                    ; 0xc3832
    mov ax, di                                ; 89 f8                       ; 0xc3835 vbe.c:498
    call 03420h                               ; e8 e6 fb                    ; 0xc3837
    xor ax, ax                                ; 31 c0                       ; 0xc383a vbe.c:499
    call 00603h                               ; e8 c4 cd                    ; 0xc383c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc383f vbe.c:500
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3842
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3844
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc3847
    or ax, dx                                 ; 09 d0                       ; 0xc384b
    call 005ddh                               ; e8 8d cd                    ; 0xc384d
    call 006d2h                               ; e8 7f ce                    ; 0xc3850 vbe.c:501
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc3853 vbe.c:503
    mov dx, 000bah                            ; ba ba 00                    ; 0xc3856
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3859
    call 02f81h                               ; e8 22 f7                    ; 0xc385c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc385f vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3862
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3864
    mov dx, 00087h                            ; ba 87 00                    ; 0xc3867
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc386a
    call 02f65h                               ; e8 f5 f6                    ; 0xc386d
    jmp near 037e1h                           ; e9 6e ff                    ; 0xc3870
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3873 vbe.c:513
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc3876 vbe.c:517
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3879 vbe.c:518
    pop di                                    ; 5f                          ; 0xc387c
    pop si                                    ; 5e                          ; 0xc387d
    pop bp                                    ; 5d                          ; 0xc387e
    retn                                      ; c3                          ; 0xc387f
  ; disGetNextSymbol 0xc3880 LB 0x259 -> off=0x0 cb=0000000000000008 uValue=00000000000c3880 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3880 LB 0x8
    push bp                                   ; 55                          ; 0xc3880 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3881
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3883 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3886
    retn                                      ; c3                          ; 0xc3887
  ; disGetNextSymbol 0xc3888 LB 0x251 -> off=0x0 cb=000000000000005b uValue=00000000000c3888 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3888 LB 0x5b
    push bp                                   ; 55                          ; 0xc3888 vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3889
    push bx                                   ; 53                          ; 0xc388b
    push cx                                   ; 51                          ; 0xc388c
    push si                                   ; 56                          ; 0xc388d
    push di                                   ; 57                          ; 0xc388e
    push ax                                   ; 50                          ; 0xc388f
    mov di, ax                                ; 89 c7                       ; 0xc3890
    mov cx, dx                                ; 89 d1                       ; 0xc3892
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3894 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3897
    out DX, ax                                ; ef                          ; 0xc389a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc389b vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc389e
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc389f
    mov bx, ax                                ; 89 c3                       ; 0xc38a2 vbe.c:531
    mov dx, cx                                ; 89 ca                       ; 0xc38a4
    mov ax, di                                ; 89 f8                       ; 0xc38a6
    call 02f81h                               ; e8 d6 f6                    ; 0xc38a8
    inc cx                                    ; 41                          ; 0xc38ab vbe.c:532
    inc cx                                    ; 41                          ; 0xc38ac
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc38ad vbe.c:533
    je short 038dah                           ; 74 27                       ; 0xc38b1
    mov si, strict word 00001h                ; be 01 00                    ; 0xc38b3 vbe.c:535
    jmp short 038bdh                          ; eb 05                       ; 0xc38b6
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc38b8
    jnbe short 038dah                         ; 77 1d                       ; 0xc38bb
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc38bd vbe.c:536
    je short 038d7h                           ; 74 15                       ; 0xc38c0
    mov ax, si                                ; 89 f0                       ; 0xc38c2 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38c4
    out DX, ax                                ; ef                          ; 0xc38c7
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38c8 vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc38cb
    mov bx, ax                                ; 89 c3                       ; 0xc38cc
    mov dx, cx                                ; 89 ca                       ; 0xc38ce
    mov ax, di                                ; 89 f8                       ; 0xc38d0
    call 02f81h                               ; e8 ac f6                    ; 0xc38d2
    inc cx                                    ; 41                          ; 0xc38d5 vbe.c:539
    inc cx                                    ; 41                          ; 0xc38d6
    inc si                                    ; 46                          ; 0xc38d7 vbe.c:541
    jmp short 038b8h                          ; eb de                       ; 0xc38d8
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc38da vbe.c:542
    pop di                                    ; 5f                          ; 0xc38dd
    pop si                                    ; 5e                          ; 0xc38de
    pop cx                                    ; 59                          ; 0xc38df
    pop bx                                    ; 5b                          ; 0xc38e0
    pop bp                                    ; 5d                          ; 0xc38e1
    retn                                      ; c3                          ; 0xc38e2
  ; disGetNextSymbol 0xc38e3 LB 0x1f6 -> off=0x0 cb=000000000000009b uValue=00000000000c38e3 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc38e3 LB 0x9b
    push bp                                   ; 55                          ; 0xc38e3 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc38e4
    push bx                                   ; 53                          ; 0xc38e6
    push cx                                   ; 51                          ; 0xc38e7
    push si                                   ; 56                          ; 0xc38e8
    push ax                                   ; 50                          ; 0xc38e9
    mov cx, ax                                ; 89 c1                       ; 0xc38ea
    mov bx, dx                                ; 89 d3                       ; 0xc38ec
    call 02f73h                               ; e8 82 f6                    ; 0xc38ee vbe.c:549
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc38f1
    inc bx                                    ; 43                          ; 0xc38f4 vbe.c:550
    inc bx                                    ; 43                          ; 0xc38f5
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc38f6 vbe.c:552
    jne short 0390ch                          ; 75 10                       ; 0xc38fa
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc38fc vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38ff
    out DX, ax                                ; ef                          ; 0xc3902
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3903 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3906
    out DX, ax                                ; ef                          ; 0xc3909
    jmp short 03976h                          ; eb 6a                       ; 0xc390a vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc390c vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc390f
    out DX, ax                                ; ef                          ; 0xc3912
    mov dx, bx                                ; 89 da                       ; 0xc3913 vbe.c:557
    mov ax, cx                                ; 89 c8                       ; 0xc3915
    call 02f73h                               ; e8 59 f6                    ; 0xc3917
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc391a
    out DX, ax                                ; ef                          ; 0xc391d
    inc bx                                    ; 43                          ; 0xc391e vbe.c:558
    inc bx                                    ; 43                          ; 0xc391f
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3920
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3923
    out DX, ax                                ; ef                          ; 0xc3926
    mov dx, bx                                ; 89 da                       ; 0xc3927 vbe.c:560
    mov ax, cx                                ; 89 c8                       ; 0xc3929
    call 02f73h                               ; e8 45 f6                    ; 0xc392b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc392e
    out DX, ax                                ; ef                          ; 0xc3931
    inc bx                                    ; 43                          ; 0xc3932 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3933
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3934
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3937
    out DX, ax                                ; ef                          ; 0xc393a
    mov dx, bx                                ; 89 da                       ; 0xc393b vbe.c:563
    mov ax, cx                                ; 89 c8                       ; 0xc393d
    call 02f73h                               ; e8 31 f6                    ; 0xc393f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3942
    out DX, ax                                ; ef                          ; 0xc3945
    inc bx                                    ; 43                          ; 0xc3946 vbe.c:564
    inc bx                                    ; 43                          ; 0xc3947
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3948
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc394b
    out DX, ax                                ; ef                          ; 0xc394e
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc394f vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3952
    out DX, ax                                ; ef                          ; 0xc3955
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3956 vbe.c:568
    jmp short 03960h                          ; eb 05                       ; 0xc3959
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc395b
    jnbe short 03976h                         ; 77 16                       ; 0xc395e
    mov ax, si                                ; 89 f0                       ; 0xc3960 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3962
    out DX, ax                                ; ef                          ; 0xc3965
    mov dx, bx                                ; 89 da                       ; 0xc3966 vbe.c:570
    mov ax, cx                                ; 89 c8                       ; 0xc3968
    call 02f73h                               ; e8 06 f6                    ; 0xc396a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc396d
    out DX, ax                                ; ef                          ; 0xc3970
    inc bx                                    ; 43                          ; 0xc3971 vbe.c:571
    inc bx                                    ; 43                          ; 0xc3972
    inc si                                    ; 46                          ; 0xc3973 vbe.c:572
    jmp short 0395bh                          ; eb e5                       ; 0xc3974
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3976 vbe.c:574
    pop si                                    ; 5e                          ; 0xc3979
    pop cx                                    ; 59                          ; 0xc397a
    pop bx                                    ; 5b                          ; 0xc397b
    pop bp                                    ; 5d                          ; 0xc397c
    retn                                      ; c3                          ; 0xc397d
  ; disGetNextSymbol 0xc397e LB 0x15b -> off=0x0 cb=000000000000008c uValue=00000000000c397e 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc397e LB 0x8c
    push bp                                   ; 55                          ; 0xc397e vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc397f
    push si                                   ; 56                          ; 0xc3981
    push di                                   ; 57                          ; 0xc3982
    push ax                                   ; 50                          ; 0xc3983
    mov si, ax                                ; 89 c6                       ; 0xc3984
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc3986
    mov ax, bx                                ; 89 d8                       ; 0xc3989
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc398b
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc398e vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc3991 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3993
    je short 039ddh                           ; 74 45                       ; 0xc3996
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3998
    je short 039c1h                           ; 74 24                       ; 0xc399b
    test ax, ax                               ; 85 c0                       ; 0xc399d
    jne short 039f9h                          ; 75 58                       ; 0xc399f
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc39a1 vbe.c:598
    call 0286ch                               ; e8 c5 ee                    ; 0xc39a4
    mov cx, ax                                ; 89 c1                       ; 0xc39a7
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc39a9 vbe.c:602
    je short 039b4h                           ; 74 05                       ; 0xc39ad
    call 03880h                               ; e8 ce fe                    ; 0xc39af vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc39b2
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc39b4 vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc39b7
    push SS                                   ; 16                          ; 0xc39ba
    pop ES                                    ; 07                          ; 0xc39bb
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc39bc
    jmp short 039fch                          ; eb 3b                       ; 0xc39bf vbe.c:605
    push SS                                   ; 16                          ; 0xc39c1 vbe.c:607
    pop ES                                    ; 07                          ; 0xc39c2
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc39c3
    mov dx, cx                                ; 89 ca                       ; 0xc39c6 vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc39c8
    call 028a1h                               ; e8 d3 ee                    ; 0xc39cb
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc39ce vbe.c:612
    je short 039fch                           ; 74 28                       ; 0xc39d2
    mov dx, ax                                ; 89 c2                       ; 0xc39d4 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc39d6
    call 03888h                               ; e8 ad fe                    ; 0xc39d8
    jmp short 039fch                          ; eb 1f                       ; 0xc39db vbe.c:614
    push SS                                   ; 16                          ; 0xc39dd vbe.c:616
    pop ES                                    ; 07                          ; 0xc39de
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc39df
    mov dx, cx                                ; 89 ca                       ; 0xc39e2 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc39e4
    call 02c0ah                               ; e8 20 f2                    ; 0xc39e7
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc39ea vbe.c:621
    je short 039fch                           ; 74 0c                       ; 0xc39ee
    mov dx, ax                                ; 89 c2                       ; 0xc39f0 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc39f2
    call 038e3h                               ; e8 ec fe                    ; 0xc39f4
    jmp short 039fch                          ; eb 03                       ; 0xc39f7 vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc39f9 vbe.c:626
    push SS                                   ; 16                          ; 0xc39fc vbe.c:629
    pop ES                                    ; 07                          ; 0xc39fd
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc39fe
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3a01 vbe.c:630
    pop di                                    ; 5f                          ; 0xc3a04
    pop si                                    ; 5e                          ; 0xc3a05
    pop bp                                    ; 5d                          ; 0xc3a06
    retn 00002h                               ; c2 02 00                    ; 0xc3a07
  ; disGetNextSymbol 0xc3a0a LB 0xcf -> off=0x0 cb=00000000000000cf uValue=00000000000c3a0a 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc3a0a LB 0xcf
    push bp                                   ; 55                          ; 0xc3a0a vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc3a0b
    push si                                   ; 56                          ; 0xc3a0d
    push di                                   ; 57                          ; 0xc3a0e
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc3a0f
    push ax                                   ; 50                          ; 0xc3a12
    mov di, dx                                ; 89 d7                       ; 0xc3a13
    mov si, bx                                ; 89 de                       ; 0xc3a15
    mov word [bp-008h], cx                    ; 89 4e f8                    ; 0xc3a17
    call 03477h                               ; e8 5a fa                    ; 0xc3a1a vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3a1d vbe.c:661
    jne short 03a26h                          ; 75 05                       ; 0xc3a1f
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3a21
    jmp short 03a29h                          ; eb 03                       ; 0xc3a24
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3a26
    call 034afh                               ; e8 83 fa                    ; 0xc3a29 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3a2c
    mov word [bp-006h], strict word 0004fh    ; c7 46 fa 4f 00              ; 0xc3a2f vbe.c:663
    push SS                                   ; 16                          ; 0xc3a34 vbe.c:664
    pop ES                                    ; 07                          ; 0xc3a35
    mov bx, word [es:si]                      ; 26 8b 1c                    ; 0xc3a36
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3a39 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc3a3c vbe.c:669
    je short 03a4bh                           ; 74 0b                       ; 0xc3a3e
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc3a40
    je short 03a72h                           ; 74 2e                       ; 0xc3a42
    test al, al                               ; 84 c0                       ; 0xc3a44
    je short 03a6dh                           ; 74 25                       ; 0xc3a46
    jmp near 03ac2h                           ; e9 77 00                    ; 0xc3a48
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3a4b vbe.c:671
    jne short 03a55h                          ; 75 05                       ; 0xc3a4e
    sal bx, 003h                              ; c1 e3 03                    ; 0xc3a50 vbe.c:672
    jmp short 03a6dh                          ; eb 18                       ; 0xc3a53 vbe.c:673
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3a55 vbe.c:674
    cwd                                       ; 99                          ; 0xc3a58
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3a59
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3a5c
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3a5e
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3a61
    mov ax, bx                                ; 89 d8                       ; 0xc3a64
    xor dx, dx                                ; 31 d2                       ; 0xc3a66
    div word [bp-00ch]                        ; f7 76 f4                    ; 0xc3a68
    mov bx, ax                                ; 89 c3                       ; 0xc3a6b
    mov ax, bx                                ; 89 d8                       ; 0xc3a6d vbe.c:677
    call 03490h                               ; e8 1e fa                    ; 0xc3a6f
    call 034afh                               ; e8 3a fa                    ; 0xc3a72 vbe.c:680
    mov bx, ax                                ; 89 c3                       ; 0xc3a75
    push SS                                   ; 16                          ; 0xc3a77 vbe.c:681
    pop ES                                    ; 07                          ; 0xc3a78
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3a79
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3a7c vbe.c:682
    jne short 03a86h                          ; 75 05                       ; 0xc3a7f
    shr bx, 003h                              ; c1 eb 03                    ; 0xc3a81 vbe.c:683
    jmp short 03a95h                          ; eb 0f                       ; 0xc3a84 vbe.c:684
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3a86 vbe.c:685
    cwd                                       ; 99                          ; 0xc3a89
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3a8a
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3a8d
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3a8f
    imul bx, ax                               ; 0f af d8                    ; 0xc3a92
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc3a95 vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc3a98
    push SS                                   ; 16                          ; 0xc3a9b vbe.c:687
    pop ES                                    ; 07                          ; 0xc3a9c
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc3a9d
    call 034c8h                               ; e8 25 fa                    ; 0xc3aa0 vbe.c:688
    push SS                                   ; 16                          ; 0xc3aa3
    pop ES                                    ; 07                          ; 0xc3aa4
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3aa5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3aa8
    call 0343fh                               ; e8 91 f9                    ; 0xc3aab vbe.c:689
    push SS                                   ; 16                          ; 0xc3aae
    pop ES                                    ; 07                          ; 0xc3aaf
    cmp ax, word [es:bx]                      ; 26 3b 07                    ; 0xc3ab0
    jbe short 03ac7h                          ; 76 12                       ; 0xc3ab3
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ab5 vbe.c:690
    call 03490h                               ; e8 d5 f9                    ; 0xc3ab8
    mov word [bp-006h], 00200h                ; c7 46 fa 00 02              ; 0xc3abb vbe.c:691
    jmp short 03ac7h                          ; eb 05                       ; 0xc3ac0 vbe.c:693
    mov word [bp-006h], 00100h                ; c7 46 fa 00 01              ; 0xc3ac2 vbe.c:696
    push SS                                   ; 16                          ; 0xc3ac7 vbe.c:699
    pop ES                                    ; 07                          ; 0xc3ac8
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3ac9
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc3acc
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3acf
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ad2 vbe.c:700
    pop di                                    ; 5f                          ; 0xc3ad5
    pop si                                    ; 5e                          ; 0xc3ad6
    pop bp                                    ; 5d                          ; 0xc3ad7
    retn                                      ; c3                          ; 0xc3ad8

  ; Padding 0x927 bytes at 0xc3ad9
  times 2343 db 0

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
    db  069h, 06fh, 073h, 033h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h
    db  06fh, 073h, 033h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 071h
