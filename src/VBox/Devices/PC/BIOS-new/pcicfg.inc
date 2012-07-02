
BX_PCIBIOS		equ	1		; Enable PCI BIOS

PCI_CFG1        	equ     0CF8h
PCI_CFG2        	equ     0CFCh

PCI_FIXED_HOST_BRIDGE_1 equ	12378086h	;; i440FX PCI bridge
PCI_FIXED_HOST_BRIDGE_2 equ	24488086h	;; ICH9 PCI bridge

MAX_BUSDEVFN		equ	10000h		; Max bus/dev/fn to search
