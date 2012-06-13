; Solaris kernel modules use non-standard ELF constructions to express inter-
; module dependencies, namely a DT_NEEDED tag inside a relocatable ELF file.
; The Solaris linker can generate these automatically; since yasm can't
; produce an ELF file which quite fits Solaris's requirements we create one
; manually using flat binary output format.
;
;; @todo Generate this.

BITS 64

elf_hdr:          ; elf64_hdr structure
 db 7fh, "ELF"    ; e_ident
 dd 010102h, 0, 0 ; e_ident
 dw 1             ; e_type ET_REL
 dw 62            ; e_machine EM_X86_64
 dd 1             ; e_version EV_CURRENT
 dq 0             ; e_entry
 dq 0             ; e_phoff
 dq sect_hdr - $$ ; e_shoff
 dd 0             ; e_flags
 dw elf_hsize     ; e_ehsize
 dw 0             ; e_phentsize
 dw 0             ; e_phnum
 dw 40h           ; e_shentsize
 dw 4             ; e_shnum
 dw 1             ; e_shstrndx section .shstrtab
elf_hsize equ $ - elf_hdr

sect_hdr:         ; elf64_shdr structure
 times 40h db 0   ; undefined section

 dd str_shstrtab  ; sh_name .shstrtab
 dd 3             ; sh_type SHT_STRTAB
 dq 20h           ; sh_flags SHF_STRINGS
 dq 0             ; sh_addr
 dq shstrtab - $$ ; sh_offset
 dq shstrtab_size ; sh_size
 dd 0             ; sh_link
 dd 0             ; sh_info
 dq 1             ; sh_addralign
 dq 0             ; sh_entsize

 dd str_dynstr    ; sh_name .dynstr
 dd 3             ; sh_type SHT_STRTAB
 dq 20h           ; sh_flags SHF_STRINGS
 dq 0             ; sh_addr
 dq dynstr - $$   ; sh_offset
 dq dynstr_size   ; sh_size
 dd 0             ; sh_link
 dd 0             ; sh_info
 dq 1             ; sh_addralign
 dq 0             ; sh_entsize

 dd str_dynamic   ; sh_name .dynamic
 dd 6             ; sh_type SHT_DYNAMIC
 dq 1             ; sh_flags SHF_WRITE
 dq 0             ; sh_addr
 dq dynamic - $$  ; sh_offset
 dq dynamic_size  ; sh_size
 dd 2             ; sh_link .dynstr
 dd 0             ; sh_info
 dq 8             ; sh_addralign
 dq 0             ; sh_entsize
 
shstrtab:
str_shstrtab equ $ - shstrtab
 db ".shstrtab", 0
str_dynstr equ $ - shstrtab
 db ".dynstr", 0
str_dynamic equ $ - shstrtab
 db ".dynamic", 0
shstrtab_size equ $ - shstrtab

dynstr:
 db 0
str_misc_ctf equ $ - dynstr
 db "misc/ctf", 0
dynstr_size equ $ - dynstr

dynamic:
 dd 1, 0         ; DT_NEEDED
 dd str_misc_ctf
 dd 0            ; padding
 dd 1ah, 0       ; DT_FLAGS
 dd 4, 0         ; TEXTREL
 dd 6ffffffbh, 0 ; DT_FLAGS1
 dd 0, 0
 dd 601900h, 0   ; SUNW_STRPAD
 dd 200h, 0
 dd 601b00h, 0   ; SUNW_LDMACH
 dd 0x3e, 0
 dd 0, 0, 0, 0   ; padding 5
 dd 0, 0, 0, 0   ; padding 6
 dd 0, 0, 0, 0   ; padding 7
 dd 0, 0, 0, 0   ; padding 8
 dd 0, 0, 0, 0   ; padding 9
 dd 0, 0, 0, 0   ; padding 10
 dd 0, 0, 0, 0   ; padding 11
 dd 0, 0, 0, 0   ; padding 12
 dd 0, 0, 0, 0   ; padding 13
 dd 0, 0, 0, 0   ; padding 14
 dd 0, 0, 0, 0   ; padding 15
dynamic_size equ $ - dynamic

