/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h,v 1.119 2002/01/16 02:00:43 martin Exp $ */
/*
 * PCI Probe
 *
 * Copyright 1995-2000 by The XFree86 Project, Inc.
 *
 * A lot of this comes from Robin Cutshaw's scanpci
 *
 * Notes -- Jun 6, 2000 -- Kevin Brosius
 * Tips on adding Entries:
 *   scanpci output can contain up to 4 numeric entries, 2 for chip and 2 for card
 *   some generic cards don't have any valid info in the card field,
 *   here's what you do;
 *   - Add a vendor entry for your device if it doesn't already exist.  The
 *     first number of the pair is generally vendor id.  Search for it below
 *     and add a #define for it if it doesn't exist.
 *       ie. 5333 is the vendor id for S3
 *   - Go to xf86PCIVendorNameInfoData[] and add a text name for your vendor id.
 *       ie. PCI_VENDOR_S3 is "S3"
 *   - Add an entry to xf86PCIVendorInfoData[], using the PCI_VENDOR define
 *     you added, and a text description of the chipset.
 *   - If your device has 0000 in the card field,
 *     you've probably got a non-video or generic device.  Stop here.
 *   
 *   - If you have info in the card field, and it's just a duplicate of the chip
 *     info, then either stop, or add a 'generic' entry to xf86PCICardInfoData[].
 *   - If you have different info in the card field, check the first entry,
 *     does the vendor match and/or already exist?  If not, add it.  Then
 *     add an entry describing the card to xf86PCICardInfoData[]
 *   - If you are adding a video card, add a PCI_CHIP #define matching the second
 *     entry in your chip field.  This gets used in your card driver as the PCI id.
 *       ie. under the S3 comment, one entry is: PCI_CHIP_VIRGE 0x5631
 *       
 * Several people recommended http://www.yourvote.com/pci for pci device/vendor info.
 * 
 */

#ifndef _XF86_PCIINFO_H
#define _XF86_PCIINFO_H

#ifndef SELF_CONTAINED_PCI_INFO
#include "xf86str.h"
#endif

/* PCI Pseudo Vendor */
#define PCI_VENDOR_GENERIC      0x00FF

#define PCI_VENDOR_REAL3D	0x003D
#define PCI_VENDOR_COMPAQ	0x0E11
#define PCI_VENDOR_NCR_1	0x1000
#define PCI_VENDOR_ATI		0x1002
#define PCI_VENDOR_VLSI		0x1004
#define PCI_VENDOR_AVANCE	0x1005
#define PCI_VENDOR_NS		0x100B
#define PCI_VENDOR_TSENG	0x100C
#define PCI_VENDOR_WEITEK	0x100E
#define PCI_VENDOR_VIDEOLOGIC	0x1010
#define PCI_VENDOR_DIGITAL	0x1011
#define PCI_VENDOR_CIRRUS	0x1013
#define PCI_VENDOR_IBM		0x1014
#define PCI_VENDOR_NCR_2	0x101A
#define PCI_VENDOR_WD		0x101C
#define PCI_VENDOR_AMD		0x1022
#define PCI_VENDOR_TRIDENT	0x1023
#define PCI_VENDOR_ALI		0x1025
#define PCI_VENDOR_MATROX	0x102B
#define PCI_VENDOR_CHIPSTECH	0x102C
#define PCI_VENDOR_MIRO		0x1031
#define PCI_VENDOR_NEC		0x1033
#define PCI_VENDOR_FD		0x1036
#define PCI_VENDOR_SIS		0x1039
#define PCI_VENDOR_HP		0x103C
#define PCI_VENDOR_SMC_PCTECH	0x1042
#define PCI_VENDOR_DPT		0x1044
#define PCI_VENDOR_OPTI		0x1045
#define PCI_VENDOR_ELSA		0x1048
#define PCI_VENDOR_SGS		0x104A
#define PCI_VENDOR_BUSLOGIC	0x104B
#define PCI_VENDOR_TI		0x104C
#define PCI_VENDOR_SONY		0x104D
#define PCI_VENDOR_OAK		0x104E
#define PCI_VENDOR_WINBOND	0x1050
#define PCI_VENDOR_MOTOROLA	0x1057
#define PCI_VENDOR_PROMISE	0x105A
#define PCI_VENDOR_NUMNINE	0x105D
#define PCI_VENDOR_UMC		0x1060
#define PCI_VENDOR_X		0x1061
#define PCI_VENDOR_PICOP	0x1066
#define PCI_VENDOR_MYLEX	0x1069
#define PCI_VENDOR_APPLE	0x106B
#define PCI_VENDOR_YAMAHA	0x1073
#define PCI_VENDOR_NEXGEN	0x1074
#define PCI_VENDOR_QLOGIC	0x1077
#define PCI_VENDOR_CYRIX	0x1078
#define PCI_VENDOR_LEADTEK	0x107D
#define PCI_VENDOR_CONTAQ	0x1080
#define PCI_VENDOR_FOREX	0x1083
#define PCI_VENDOR_SBS		0x108A
#define PCI_VENDOR_OLICOM	0x108D
#define PCI_VENDOR_SUN		0x108E
#define PCI_VENDOR_DIAMOND	0x1092
#define PCI_VENDOR_CMD		0x1095
#define PCI_VENDOR_APPIAN	0x1097
#define PCI_VENDOR_VISION	0x1098
#define PCI_VENDOR_BROOKTREE	0x109E
#define PCI_VENDOR_SIERRA	0x10A8
#define PCI_VENDOR_ACC		0x10AA
#define PCI_VENDOR_WINBOND_2	0x10AB
#define PCI_VENDOR_DATABOOK	0x10B3
#define PCI_VENDOR_3COM		0x10B7
#define PCI_VENDOR_SMC		0x10B8
#define PCI_VENDOR_ALI_2	0x10B9
#define PCI_VENDOR_MITSUBISHI	0x10BA
#define PCI_VENDOR_SURECOM	0x10BD
#define PCI_VENDOR_NEOMAGIC	0x10C8
#define PCI_VENDOR_ASP		0x10CD
#define PCI_VENDOR_CERN		0x10DC
#define PCI_VENDOR_NVIDIA	0x10DE
#define PCI_VENDOR_IMS		0x10E0
#define PCI_VENDOR_TEKRAM	0x10E1
#define PCI_VENDOR_TUNDRA	0x10E3
#define PCI_VENDOR_AMCC		0x10E8
#define PCI_VENDOR_INTEGRAPHICS	0x10EA
#define PCI_VENDOR_REALTEC	0x10EC
#define PCI_VENDOR_TRUEVISION	0x10FA
#define PCI_VENDOR_INITIO	0x1101
#define PCI_VENDOR_CREATIVE_2	0x1102
#define PCI_VENDOR_SIGMADESIGNS_2	0x1105
#define PCI_VENDOR_VIA		0x1106
#define PCI_VENDOR_VORTEX	0x1119
#define PCI_VENDOR_EF		0x111A
#define PCI_VENDOR_FORE		0x1127
#define PCI_VENDOR_IMAGTEC	0x112F
#define PCI_VENDOR_PLX		0x113C
#define PCI_VENDOR_ALLIANCE	0x1142
#define PCI_VENDOR_VMIC		0x114A
#define PCI_VENDOR_DIGI		0x114F
#define PCI_VENDOR_MUTECH	0x1159
#define PCI_VENDOR_RENDITION	0x1163
#define PCI_VENDOR_TOSHIBA	0x1179
#define PCI_VENDOR_RICOH	0x1180
#define PCI_VENDOR_ZEINET	0x1193
#define PCI_VENDOR_LITEON	0x11AD
#define PCI_VENDOR_SPECIALIX	0x11CB
#define PCI_VENDOR_CONTROL	0x11FE
#define PCI_VENDOR_CYCLADES	0x120E
#define PCI_VENDOR_3DFX		0x121A
#define PCI_VENDOR_SIGMADESIGNS	0x1236
#define PCI_VENDOR_SMI		0x126f
#define PCI_VENDOR_ENSONIQ	0x1274
#define PCI_VENDOR_ROCKWELL	0x127A
#define PCI_VENDOR_YOKOGAWA	0x1281
#define PCI_VENDOR_TRITECH	0x1292
#define PCI_VENDOR_NVIDIA_SGS	0x12d2
#define PCI_VENDOR_NETGEAR	0x1385
#define PCI_VENDOR_VMWARE	0x15AD
#define PCI_VENDOR_SYMPHONY	0x1C1C
#define PCI_VENDOR_TEKRAM_2	0x1DE1
#define PCI_VENDOR_3DLABS	0x3D3D
#define PCI_VENDOR_AVANCE_2	0x4005
#define PCI_VENDOR_HERCULES	0x4843
#define PCI_VENDOR_CREATIVE	0x4942
#define PCI_VENDOR_S3		0x5333
#define PCI_VENDOR_INTEL	0x8086
#define PCI_VENDOR_ADAPTEC	0x9004
#define PCI_VENDOR_ADAPTEC_2	0x9005
#define PCI_VENDOR_ATRONICS	0x907F
#define PCI_VENDOR_ARK		0xEDD8


/* Generic */
#define PCI_CHIP_VGA            0x0000
#define PCI_CHIP_8514           0x0001

/* Real 3D */
#define PCI_CHIP_I740_PCI	0x00D1

/* Compaq */
#define PCI_CHIP_QV1280		0x3033
#define PCI_CHIP_SMART		0xAE10
#define PCI_CHIP_NETELL100	0xAE32
#define PCI_CHIP_NETELL10	0xAE34
#define PCI_CHIP_NETFLEX3	0xAE35
#define PCI_CHIP_NETELL100D	0xAE40
#define PCI_CHIP_NETELL100PL	0xAE43
#define PCI_CHIP_NETELL100I	0xB011
#define PCI_CHIP_THUNDERLAN	0xF130
#define PCI_CHIP_NETFLEX3BNC	0xF150

/* NCR */
#define PCI_CHIP_53C810		0x0001
#define PCI_CHIP_53C820		0x0002
#define PCI_CHIP_53C825		0x0003
#define PCI_CHIP_53C815		0x0004
#define PCI_CHIP_53C810AP	0x0005
#define PCI_CHIP_53C860		0x0006
#define PCI_CHIP_53C896		0x000B
#define PCI_CHIP_53C895		0x000C
#define PCI_CHIP_53C885		0x000D
#define PCI_CHIP_53C875		0x000F
#define PCI_CHIP_53C875J	0x008F

/* ATI */
#define PCI_CHIP_MACH32		0x4158
#define PCI_CHIP_R200_BB	0x4242
#define PCI_CHIP_MACH64CT	0x4354
#define PCI_CHIP_MACH64CX	0x4358
#define PCI_CHIP_MACH64ET	0x4554
#define PCI_CHIP_MACH64GB	0x4742
#define PCI_CHIP_MACH64GD	0x4744
#define PCI_CHIP_MACH64GI	0x4749
#define PCI_CHIP_MACH64GL	0x474C
#define PCI_CHIP_MACH64GM	0x474D
#define PCI_CHIP_MACH64GN	0x474E
#define PCI_CHIP_MACH64GO	0x474F
#define PCI_CHIP_MACH64GP	0x4750
#define PCI_CHIP_MACH64GQ	0x4751
#define PCI_CHIP_MACH64GR	0x4752
#define PCI_CHIP_MACH64GS	0x4753
#define PCI_CHIP_MACH64GT	0x4754
#define PCI_CHIP_MACH64GU	0x4755
#define PCI_CHIP_MACH64GV	0x4756
#define PCI_CHIP_MACH64GW	0x4757
#define PCI_CHIP_MACH64GX	0x4758
#define PCI_CHIP_MACH64GY	0x4759
#define PCI_CHIP_MACH64GZ	0x475A
#define PCI_CHIP_MACH64LB	0x4C42
#define PCI_CHIP_MACH64LD	0x4C44
#define PCI_CHIP_RAGE128LE	0x4C45
#define PCI_CHIP_RAGE128LF	0x4C46
#define PCI_CHIP_MACH64LG	0x4C47
#define PCI_CHIP_MACH64LI	0x4C49
#define PCI_CHIP_MACH64LM	0x4C4D
#define PCI_CHIP_MACH64LN	0x4C4E
#define PCI_CHIP_MACH64LP	0x4C50
#define PCI_CHIP_MACH64LQ	0x4C51
#define PCI_CHIP_MACH64LR	0x4C52
#define PCI_CHIP_MACH64LS	0x4C53
#define PCI_CHIP_RADEON_LW	0x4C57
#define PCI_CHIP_RADEON_LY	0x4C59
#define PCI_CHIP_RADEON_LZ	0x4C5A
#define PCI_CHIP_RAGE128MF	0x4D46
#define PCI_CHIP_RAGE128ML	0x4D4C
#define PCI_CHIP_RAGE128PD	0x5044
#define PCI_CHIP_RAGE128PF	0x5046
#define PCI_CHIP_RAGE128PP	0x5050
#define PCI_CHIP_RAGE128PR	0x5052
#define PCI_CHIP_RADEON_QD	0x5144
#define PCI_CHIP_RADEON_QE	0x5145
#define PCI_CHIP_RADEON_QF	0x5146
#define PCI_CHIP_RADEON_QG	0x5147
#define PCI_CHIP_R200_QL	0x514C
#define PCI_CHIP_R200_QN	0x514E
#define PCI_CHIP_R200_QO	0x514F
#define PCI_CHIP_RV200_QW	0x5157
#define PCI_CHIP_RADEON_QY	0x5159
#define PCI_CHIP_RADEON_QZ	0x515A
#define PCI_CHIP_R200_Ql	0x516C
#define PCI_CHIP_RAGE128RE	0x5245
#define PCI_CHIP_RAGE128RF	0x5246
#define PCI_CHIP_RAGE128RG	0x5247
#define PCI_CHIP_RAGE128RK	0x524B
#define PCI_CHIP_RAGE128RL	0x524C
#define PCI_CHIP_RAGE128TF	0x5446
#define PCI_CHIP_RAGE128TL	0x544C
#define PCI_CHIP_RAGE128TR	0x5452
#define PCI_CHIP_RAGE128SM	0x534D
#define PCI_CHIP_MACH64VT	0x5654
#define PCI_CHIP_MACH64VU	0x5655
#define PCI_CHIP_MACH64VV	0x5656

/* VLSI */
#define PCI_CHIP_82C592_FC1	0x0005
#define PCI_CHIP_82C593_FC1	0x0006
#define PCI_CHIP_82C594_AFC2	0x0007
#define PCI_CHIP_82C597_AFC2	0x0009
#define PCI_CHIP_82C541		0x000C
#define PCI_CHIP_82C543		0x000D
#define PCI_CHIP_VAS96011  	0x0702

/* Avance Logic */
#define PCI_CHIP_ALG2064	0x2064
#define PCI_CHIP_ALG2301	0x2301
#define PCI_CHIP_ALG2501	0x2501

/* NS */
#define PCI_CHIP_87415		0x0002
#define PCI_CHIP_87410		0xD001

/* Tseng */
#define PCI_CHIP_ET4000_W32P_A	0x3202
#define PCI_CHIP_ET4000_W32P_B	0x3205
#define PCI_CHIP_ET4000_W32P_D	0x3206
#define PCI_CHIP_ET4000_W32P_C	0x3207
#define PCI_CHIP_ET6000		0x3208
#define PCI_CHIP_ET6300		0x4702

/* Weitek */
#define PCI_CHIP_P9000		0x9001
#define PCI_CHIP_P9100		0x9100

/* Digital */
#define PCI_CHIP_DC21050	0x0001
#define PCI_CHIP_DC21040_10	0x0002
#define PCI_CHIP_DEC21030	0x0004
#define PCI_CHIP_DC21040_100	0x0009
#define PCI_CHIP_TGA2    	0x000D
#define PCI_CHIP_DEFPA   	0x000F
#define PCI_CHIP_DC21041    	0x0014
#define PCI_CHIP_DC21142   	0x0019
#define PCI_CHIP_DC21052 	0x0021
#define PCI_CHIP_DC21152 	0x0024

/* Cirrus Logic */
#define PCI_CHIP_GD7548		0x0038
#define PCI_CHIP_GD7555		0x0040
#define PCI_CHIP_GD5430		0x00A0
#define PCI_CHIP_GD5434_4	0x00A4
#define PCI_CHIP_GD5434_8	0x00A8
#define PCI_CHIP_GD5436		0x00AC
#define PCI_CHIP_GD5446         0x00B8
#define PCI_CHIP_GD5480         0x00BC
#define PCI_CHIP_GD5462		0x00D0
#define PCI_CHIP_GD5464		0x00D4
#define PCI_CHIP_GD5464BD	0x00D5
#define PCI_CHIP_GD5465		0x00D6
#define PCI_CHIP_6729		0x1100
#define PCI_CHIP_6832		0x1110
#define PCI_CHIP_GD7542		0x1200
#define PCI_CHIP_GD7543		0x1202
#define PCI_CHIP_GD7541		0x1204

/* IBM */
#define PCI_CHIP_FIRE_CORAL	0x000A
#define PCI_CHIP_TOKEN_RING	0x0018
#define PCI_CHIP_82G2675	0x001D
#define PCI_CHIP_82351		0x0022

/* WD */
#define PCI_CHIP_7197		0x3296

/* AMD */
#define PCI_CHIP_79C970		0x2000
#define PCI_CHIP_53C974		0x2020

/* Trident */
#define PCI_CHIP_8400		0x8400
#define PCI_CHIP_8420		0x8420
#define PCI_CHIP_8500		0x8500
#define PCI_CHIP_8520		0x8520
#define PCI_CHIP_8600		0x8600
#define PCI_CHIP_8620		0x8620
#define PCI_CHIP_8800		0x8800
#define PCI_CHIP_8820		0x8820
#define PCI_CHIP_9320		0x9320
#define PCI_CHIP_9388		0x9388
#define PCI_CHIP_9397		0x9397
#define PCI_CHIP_939A		0x939A
#define PCI_CHIP_9420		0x9420
#define PCI_CHIP_9440		0x9440
#define PCI_CHIP_9520		0x9520
#define PCI_CHIP_9525		0x9525
#define PCI_CHIP_9540		0x9540
#define PCI_CHIP_9660		0x9660
#define PCI_CHIP_9750		0x9750
#define PCI_CHIP_9850		0x9850
#define PCI_CHIP_9880		0x9880
#define PCI_CHIP_9910		0x9910
#define PCI_CHIP_9930		0x9930

/* ALI */
#define PCI_CHIP_M1435		0x1435

/* Matrox */
#define PCI_CHIP_MGA2085	0x0518
#define PCI_CHIP_MGA2064	0x0519
#define PCI_CHIP_MGA1064	0x051a
#define PCI_CHIP_MGA2164	0x051b
#define PCI_CHIP_MGA2164_AGP	0x051f
#define PCI_CHIP_MGAG200_PCI	0x0520
#define PCI_CHIP_MGAG200	0x0521
#define PCI_CHIP_MGAG400	0x0525
#define PCI_CHIP_MGAG550	0x2527
#define PCI_CHIP_IMPRESSION	0x0D10
#define PCI_CHIP_MGAG100_PCI	0x1000
#define PCI_CHIP_MGAG100	0x1001

#define PCI_CARD_G400_TH	0x2179
#define PCI_CARD_MILL_G200_SD	0xff00
#define PCI_CARD_PROD_G100_SD	0xff01
#define PCI_CARD_MYST_G200_SD	0xff02
#define PCI_CARD_MILL_G200_SG	0xff03
#define PCI_CARD_MARV_G200_SD	0xff04

/* Chips & Tech */
#define PCI_CHIP_65545		0x00D8
#define PCI_CHIP_65548		0x00DC
#define PCI_CHIP_65550		0x00E0
#define PCI_CHIP_65554		0x00E4
#define PCI_CHIP_65555		0x00E5
#define PCI_CHIP_68554		0x00F4
#define PCI_CHIP_69000		0x00C0
#define PCI_CHIP_69030		0x0C30

/* Miro */
#define PCI_CHIP_ZR36050	0x5601

/* NEC */
#define PCI_CHIP_POWER_VR	0x0046

/* FD */
#define PCI_CHIP_TMC_18C30	0x0000

/* SiS */
#define PCI_CHIP_SG86C201	0x0001
#define PCI_CHIP_SG86C202	0x0002
#define PCI_CHIP_SG85C503	0x0008
#define PCI_CHIP_SIS5597	0x0200
#define PCI_CHIP_SG86C205	0x0205
#define PCI_CHIP_SG86C215	0x0215
#define PCI_CHIP_SG86C225	0x0225
#define PCI_CHIP_85C501		0x0406
#define PCI_CHIP_85C496		0x0496
#define PCI_CHIP_85C601		0x0601
#define PCI_CHIP_85C5107	0x5107
#define PCI_CHIP_85C5511	0x5511
#define PCI_CHIP_85C5513	0x5513
#define PCI_CHIP_SIS5571	0x5571
#define PCI_CHIP_SIS5597_2	0x5597
#define PCI_CHIP_SIS530		0x6306
#define PCI_CHIP_SIS6326	0x6326
#define PCI_CHIP_SIS7001	0x7001
#define PCI_CHIP_SIS300		0x0300
#define PCI_CHIP_SIS630		0x6300
#define PCI_CHIP_SIS540		0x5300
/* Agregado por Carlos Duclos & Manuel Jander */
#define PCI_CHIP_SIS82C204	0x0204
/* HP */
#define PCI_CHIP_J2585A		0x1030
#define PCI_CHIP_J2585B		0x1031

/* SMC/PCTECH */
#define PCI_CHIP_RZ1000		0x1000
#define PCI_CHIP_RZ1001		0x1001

/* DPT */
#define PCI_CHIP_SMART_CACHE	0xA400

/* Opti */
#define PCI_CHIP_92C178		0xC178
#define PCI_CHIP_82C557		0xC557
#define PCI_CHIP_82C558		0xC558
#define PCI_CHIP_82C621		0xC621
#define PCI_CHIP_82C700		0xC700
#define PCI_CHIP_82C701		0xC701
#define PCI_CHIP_82C814		0xC814
#define PCI_CHIP_82C822		0xC822

/* SGS */
#define PCI_CHIP_STG2000	0x0008
#define PCI_CHIP_STG1764	0x0009

/* BusLogic */
#define PCI_CHIP_946C_01	0x0140
#define PCI_CHIP_946C_10	0x1040
#define PCI_CHIP_FLASH_POINT	0x8130

/* Texas Instruments */
#define PCI_CHIP_TI_PERMEDIA	0x3d04
#define PCI_CHIP_TI_PERMEDIA2	0x3d07
#define PCI_CHIP_PCI_1130	0xAC12
#define PCI_CHIP_PCI_1131	0xAC15

/* Oak */
#define PCI_CHIP_OTI107		0x0107

/* Winbond */
#define PCI_CHIP_89C940		0x0940

/* Motorola */
#define PCI_CHIP_MPC105_EAGLE	0x0001
#define PCI_CHIP_MPC105_GRACKLE	0x0002
#define PCI_CHIP_RAVEN	 	0x4801

/* Promise */
#define PCI_CHIP_ULTRA_DMA	0x4D33
#define PCI_CHIP_DC5030		0x5300

/* Number Nine */
#define PCI_CHIP_I128		0x2309
#define PCI_CHIP_I128_2		0x2339
#define PCI_CHIP_I128_T2R	0x493D
#define PCI_CHIP_I128_T2R4	0x5348

/* Sun */
#define PCI_CHIP_EBUS		0x1000
#define PCI_CHIP_HAPPY_MEAL	0x1001
#define PCI_CHIP_SIMBA		0x5000
#define PCI_CHIP_PSYCHO		0x8000
#define PCI_CHIP_SCHIZO		0x8001
#define PCI_CHIP_SABRE		0xA000
#define PCI_CHIP_HUMMINGBIRD	0xA001

/* BrookTree */
#define PCI_CHIP_BT848		0x0350
#define PCI_CHIP_BT849		0x0351

/* NVIDIA */
#define PCI_CHIP_NV1		0x0008
#define PCI_CHIP_DAC64		0x0009
#define PCI_CHIP_TNT		0x0020
#define PCI_CHIP_TNT2		0x0028
#define PCI_CHIP_UTNT2		0x0029
#define PCI_CHIP_VTNT2		0x002C
#define PCI_CHIP_UVTNT2		0x002D
#define PCI_CHIP_ITNT2		0x00A0
#define PCI_CHIP_GEFORCE256     0x0100
#define PCI_CHIP_GEFORCEDDR     0x0101
#define PCI_CHIP_QUADRO         0x0103
#define PCI_CHIP_GEFORCE2MX     0x0110
#define PCI_CHIP_GEFORCE2MXDDR  0x0111
#define PCI_CHIP_GEFORCE2GO	0x0112
#define PCI_CHIP_QUADRO2MXR     0x0113
#define PCI_CHIP_GEFORCE2GTS    0x0150
#define PCI_CHIP_GEFORCE2GTS_1  0x0151
#define PCI_CHIP_GEFORCE2ULTRA  0x0152
#define PCI_CHIP_QUADRO2PRO     0x0153
#define PCI_CHIP_0x0170         0x0170
#define PCI_CHIP_0x0171         0x0171
#define PCI_CHIP_0x0172         0x0172
#define PCI_CHIP_0x0173         0x0173
#define PCI_CHIP_0x0174         0x0174
#define PCI_CHIP_0x0175         0x0175
#define PCI_CHIP_0x0178         0x0178
#define PCI_CHIP_0x017A         0x017A
#define PCI_CHIP_0x017B         0x017B
#define PCI_CHIP_0x017C         0x017C
#define PCI_CHIP_IGEFORCE2      0x01A0
#define PCI_CHIP_GEFORCE3	0x0200
#define PCI_CHIP_GEFORCE3_1	0x0201
#define PCI_CHIP_GEFORCE3_2	0x0202
#define PCI_CHIP_QUADRO_DDC	0x0203
#define PCI_CHIP_0x0250         0x0250
#define PCI_CHIP_0x0258         0x0258

/* NVIDIA & SGS */
#define PCI_CHIP_RIVA128	0x0018

/* IMS */
#define PCI_CHIP_IMSTT128	0x9128
#define PCI_CHIP_IMSTT3D	0x9135

/* Alliance Semiconductor */
#define PCI_CHIP_AP6410		0x3210
#define PCI_CHIP_AP6422		0x6422
#define PCI_CHIP_AT24		0x6424
#define PCI_CHIP_AT3D		0x643D

/* 3dfx Interactive */
#define PCI_CHIP_VOODOO_GRAPHICS 0x0001
#define PCI_CHIP_VOODOO2	0x0002
#define PCI_CHIP_BANSHEE	0x0003
#define PCI_CHIP_VOODOO3	0x0005
#define PCI_CHIP_VOODOO5	0x0009

#define PCI_CARD_VOODOO3_2000	0x0036
#define PCI_CARD_VOODOO3_3000	0x003a

/* Rendition */
#define PCI_CHIP_V1000		0x0001
#define PCI_CHIP_V2x00		0x2000

/* 3Dlabs */
#define PCI_CHIP_300SX		0x0001
#define PCI_CHIP_500TX		0x0002
#define PCI_CHIP_DELTA		0x0003
#define PCI_CHIP_PERMEDIA	0x0004
#define PCI_CHIP_MX		0x0006
#define PCI_CHIP_PERMEDIA2	0x0007
#define PCI_CHIP_GAMMA		0x0008
#define PCI_CHIP_PERMEDIA2V	0x0009
#define PCI_CHIP_PERMEDIA3	0x000A
#define PCI_CHIP_PERMEDIA4	0x000C
#define PCI_CHIP_R4		0x000D
#define PCI_CHIP_GAMMA2		0x000E
#define PCI_CHIP_R4ALT		0x0011

/* S3 */
#define PCI_CHIP_PLATO		0x0551
#define PCI_CHIP_VIRGE		0x5631
#define PCI_CHIP_TRIO		0x8811
#define PCI_CHIP_AURORA64VP	0x8812
#define PCI_CHIP_TRIO64UVP	0x8814
#define PCI_CHIP_VIRGE_VX	0x883D
#define PCI_CHIP_868		0x8880
#define PCI_CHIP_928		0x88B0
#define PCI_CHIP_864_0		0x88C0
#define PCI_CHIP_864_1		0x88C1
#define PCI_CHIP_964_0		0x88D0
#define PCI_CHIP_964_1		0x88D1
#define PCI_CHIP_968		0x88F0
#define PCI_CHIP_TRIO64V2_DXGX	0x8901
#define PCI_CHIP_PLATO_PX	0x8902
#define PCI_CHIP_Trio3D		0x8904
#define PCI_CHIP_Trio3D_2X	0x8A13
#define PCI_CHIP_VIRGE_DXGX	0x8A01
#define PCI_CHIP_VIRGE_GX2	0x8A10
#define PCI_CHIP_SAVAGE3D	0x8A20
#define PCI_CHIP_SAVAGE3D_MV	0x8A21
#define PCI_CHIP_SAVAGE4	0x8A22
#define PCI_CHIP_SAVAGE2000	0x9102
#define PCI_CHIP_VIRGE_MX	0x8C01
#define PCI_CHIP_VIRGE_MXPLUS	0x8C01
#define PCI_CHIP_VIRGE_MXP	0x8C03
#define PCI_CHIP_PROSAVAGE_PM	0x8A25
#define PCI_CHIP_PROSAVAGE_KM	0x8A26
#define PCI_CHIP_SAVAGE_MX_MV	0x8c10
#define PCI_CHIP_SAVAGE_MX	0x8c11
#define PCI_CHIP_SAVAGE_IX_MV	0x8c12
#define PCI_CHIP_SAVAGE_IX	0x8c13

/* ARK Logic */
#define PCI_CHIP_1000PV		0xA091
#define PCI_CHIP_2000PV		0xA099
#define PCI_CHIP_2000MT		0xA0A1
#define PCI_CHIP_2000MI		0xA0A9

/* Tritech Microelectronics */
#define PCI_CHIP_TR25202	0xfc02

/* Neomagic */
#define PCI_CHIP_NM2070		0x0001
#define PCI_CHIP_NM2090		0x0002
#define PCI_CHIP_NM2093	        0x0003
#define PCI_CHIP_NM2097	        0x0083
#define PCI_CHIP_NM2160		0x0004
#define PCI_CHIP_NM2200		0x0005
#define PCI_CHIP_NM2230		0x0025
#define PCI_CHIP_NM2360		0x0006
#define PCI_CHIP_NM2380		0x0016

/* Intel */
#define PCI_CHIP_I830_M_BRIDGE		0x3575
#define PCI_CHIP_I830_M			0x3577
#define PCI_CHIP_I815_BRIDGE		0x1130
#define PCI_CHIP_I815			0x1132
#define PCI_CHIP_I810_BRIDGE		0x7120
#define PCI_CHIP_I810			0x7121
#define PCI_CHIP_I810_DC100_BRIDGE	0x7122
#define PCI_CHIP_I810_DC100		0x7123
#define PCI_CHIP_I810_E_BRIDGE		0x7124
#define PCI_CHIP_I810_E			0x7125
#define PCI_CHIP_I740_AGP		0x7800

/* Silicon Motion Inc. */
#define PCI_CHIP_SMI910		0x910
#define PCI_CHIP_SMI810		0x810
#define PCI_CHIP_SMI820		0x820
#define PCI_CHIP_SMI710		0x710
#define PCI_CHIP_SMI712		0x712
#define PCI_CHIP_SMI720		0x720

/* VMware */
#define PCI_CHIP_VMWARE0405		0x0405
#define PCI_CHIP_VMWARE0710		0x0710

/*
 * first the VendorId - VendorName mapping
 */
extern SymTabPtr xf86PCIVendorNameInfo;

#ifdef INIT_PCI_VENDOR_NAME_INFO
static SymTabRec xf86PCIVendorNameInfoData[] = {
    {PCI_VENDOR_REAL3D, "Real 3D"},
    {PCI_VENDOR_COMPAQ, "Compaq"},
    {PCI_VENDOR_NCR_1,	"NCR"},
    {PCI_VENDOR_ATI,	"ATI"},
    {PCI_VENDOR_VLSI, "VLSI"},
    {PCI_VENDOR_AVANCE,	"Avance Logic"},
    {PCI_VENDOR_NS, "NS"},
    {PCI_VENDOR_TSENG,	"Tseng Labs"},
    {PCI_VENDOR_WEITEK,	"Weitek"},
    {PCI_VENDOR_VIDEOLOGIC,	"Video Logic"},
    {PCI_VENDOR_DIGITAL, "Digital"},
    {PCI_VENDOR_CIRRUS,	"Cirrus Logic"},
    {PCI_VENDOR_IBM, "IBM"},
    {PCI_VENDOR_NCR_2,	"NCR"},
    {PCI_VENDOR_WD, "WD*"},
    {PCI_VENDOR_AMD, "AMD"},
    {PCI_VENDOR_TRIDENT, "Trident"},
    {PCI_VENDOR_ALI, "ALI"},
    {PCI_VENDOR_MATROX,	"Matrox"},
    {PCI_VENDOR_CHIPSTECH, "C&T"},
    {PCI_VENDOR_MIRO, "Miro"},
    {PCI_VENDOR_NEC, "NEC"},
    {PCI_VENDOR_FD, "FD"},
    {PCI_VENDOR_SIS,	"SiS"},
    {PCI_VENDOR_HP, "HP"},
    {PCI_VENDOR_SMC_PCTECH, "SMC/PCTECH"},
    {PCI_VENDOR_DPT, "DPT"},
    {PCI_VENDOR_SGS,	"SGS-Thomson"},
    {PCI_VENDOR_BUSLOGIC, "BusLogic"},
    {PCI_VENDOR_TI,	"Texas Instruments"},
    {PCI_VENDOR_SONY, "Sony"},
    {PCI_VENDOR_OAK,	"Oak"},
    {PCI_VENDOR_WINBOND,"Winbond"},
    {PCI_VENDOR_MOTOROLA, "Motorola"},
    {PCI_VENDOR_OAK,	"Promise"},
    {PCI_VENDOR_NUMNINE, "Number Nine"},
    {PCI_VENDOR_UMC,	"UMC"},
    {PCI_VENDOR_X , "X"},
    {PCI_VENDOR_PICOP , "PICOP"},
    {PCI_VENDOR_MYLEX, "Mylex"},
    {PCI_VENDOR_APPLE, "Apple"},
    {PCI_VENDOR_NEXGEN, "Nexgen"},
    {PCI_VENDOR_QLOGIC, "QLogic"},
    {PCI_VENDOR_CYRIX, "Cyrix"},
    {PCI_VENDOR_LEADTEK, "Leadtek"},
    {PCI_VENDOR_CONTAQ, "Contaq"},
    {PCI_VENDOR_FOREX, "FOREX"},
    {PCI_VENDOR_SBS, "SBS Technologies"}, /* Formerly Bit 3 Computer Corp */
    {PCI_VENDOR_OLICOM, "Olicom"},
    {PCI_VENDOR_SUN, "Sun"},
    {PCI_VENDOR_DIAMOND, "Diamond"},
    {PCI_VENDOR_CMD, "CMD"},
    {PCI_VENDOR_APPIAN, "Appian Graphics"},
    {PCI_VENDOR_VISION, "Vision"},
    {PCI_VENDOR_BROOKTREE,	"BrookTree"},
    {PCI_VENDOR_SIERRA, "Sierra"},
    {PCI_VENDOR_ACC, "ACC"},
    {PCI_VENDOR_WINBOND_2, "Winbond"},
    {PCI_VENDOR_DATABOOK, "Databook"},
    {PCI_VENDOR_3COM, "3COM"},
    {PCI_VENDOR_SMC, "SMC"},
    {PCI_VENDOR_ALI_2, "ALI"},
    {PCI_VENDOR_MITSUBISHI, "Mitsubishi"},
    {PCI_VENDOR_SURECOM, "Surecom"},
    {PCI_VENDOR_NEOMAGIC,	"Neomagic"},
    {PCI_VENDOR_ASP, "Advanced System Products"},
    {PCI_VENDOR_CERN, "CERN"},
    {PCI_VENDOR_NVIDIA,	"NVidia"},
    {PCI_VENDOR_IMS, "IMS"},
    {PCI_VENDOR_TEKRAM, "Tekram"},
    {PCI_VENDOR_TUNDRA, "Tundra"},
    {PCI_VENDOR_AMCC, "AMCC"},
    {PCI_VENDOR_INTEGRAPHICS, "Intergraphics"},
    {PCI_VENDOR_REALTEC, "Realtek"},
    {PCI_VENDOR_TRUEVISION, "Truevision"},
    {PCI_VENDOR_INITIO, "Initio Corp"},
    {PCI_VENDOR_CREATIVE_2, "Creative Labs"},
    {PCI_VENDOR_SIGMADESIGNS_2, "Sigma Designs"},
    {PCI_VENDOR_VIA, "VIA"},
    {PCI_VENDOR_VORTEX, "Vortex"},
    {PCI_VENDOR_EF, "EF"},
    {PCI_VENDOR_FORE, "Fore Systems"},
    {PCI_VENDOR_IMAGTEC, "Imaging Technology"},
    {PCI_VENDOR_PLX, "PLX"},
    {PCI_VENDOR_NVIDIA_SGS,	"NVidia/SGS-Thomson"},
    {PCI_VENDOR_NETGEAR,	"Netgear"},
    {PCI_VENDOR_ALLIANCE, "Alliance Semiconductor"},
    {PCI_VENDOR_VMIC, "VMIC"},
    {PCI_VENDOR_DIGI, "DIGI*"},
    {PCI_VENDOR_MUTECH, "Mutech"},
    {PCI_VENDOR_RENDITION, "Rendition"},
    {PCI_VENDOR_TOSHIBA, "Toshiba"},
    {PCI_VENDOR_RICOH,	"Ricoh"},
    {PCI_VENDOR_ZEINET,	"Zeinet"},
    {PCI_VENDOR_LITEON,	"Lite-On"},
    {PCI_VENDOR_3DFX,	"3dfx Interactive"},
    {PCI_VENDOR_SIGMADESIGNS, "Sigma Designs"},
    {PCI_VENDOR_ENSONIQ, "Ensoniq"},
    {PCI_VENDOR_ROCKWELL, "Rockwell"},
    {PCI_VENDOR_YOKOGAWA, "YOKOGAWA"},
    {PCI_VENDOR_TRITECH,	"Tritech Microelectronics"},
    {PCI_VENDOR_SYMPHONY, "Symphony"},
    {PCI_VENDOR_TEKRAM_2, "Tekram"},
    {PCI_VENDOR_3DLABS, "3Dlabs"},
    {PCI_VENDOR_AVANCE_2, "Avance"},
    {PCI_VENDOR_CREATIVE, "Creative Labs"},
    {PCI_VENDOR_S3,	"S3"},
    {PCI_VENDOR_INTEL,	"Intel"},
    {PCI_VENDOR_ADAPTEC, "Adaptec"},
    {PCI_VENDOR_ADAPTEC_2, "Adaptec"},
    {PCI_VENDOR_ATRONICS, "Atronics"},
    {PCI_VENDOR_ARK,	"ARK Logic"},
    {PCI_VENDOR_YAMAHA, "Yamaha"},
    {PCI_VENDOR_SMI,	"Silicon Motion Inc."},
    {PCI_VENDOR_VMWARE,	"VMware"},
    {0,NULL}
};
#endif

/* Increase this as required */
#define MAX_DEV_PER_VENDOR 80

typedef struct {
    unsigned short VendorID;
    struct pciDevice {
	unsigned short DeviceID;
	char *DeviceName;
	CARD16 class;
    } Device[MAX_DEV_PER_VENDOR];
} pciVendorDeviceInfo;

extern pciVendorDeviceInfo* xf86PCIVendorInfo;

#ifdef INIT_PCI_VENDOR_INFO
static pciVendorDeviceInfo xf86PCIVendorInfoData[] = {
    {PCI_VENDOR_REAL3D, {
				{PCI_CHIP_I740_PCI, 	"i740 (PCI)",0},
				{0x0000,		NULL,0}}},

#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_COMPAQ, {
				{0x3033, 	"QVision 1280/p",0 },
				{0xae10, 	"Smart-2/P RAID Controller",0},
				{0xae32, 	"Netellignet 10/100",0 },
				{0xae34, 	"Netellignet 10",0 },
				{0xae35, 	"NetFlex 3",0 },
				{0xae40, 	"Netellignet 10/100 Dual",0 },
				{0xae43, 	"Netellignet 10/100 ProLiant",0 },
				{0xb011, 	"Netellignet 10/100 Integrated",0 },
				{0xf130, 	"ThunderLAN",0 },
				{0xf150, 	"NetFlex 3 BNC",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_NCR_1,	{
				{PCI_CHIP_53C810,	"53c810",0},
				{PCI_CHIP_53C820,	"53c820",0},
				{PCI_CHIP_53C825,	"53c825",0},
				{PCI_CHIP_53C815,	"53c815",0},
				{PCI_CHIP_53C810AP,	"53c810AP",0},
				{PCI_CHIP_53C860,	"53c860",0},
				{PCI_CHIP_53C896,	"53c896",0},
				{PCI_CHIP_53C895,	"53c895",0},
				{PCI_CHIP_53C885,	"53c885",0},
				{PCI_CHIP_53C875,	"53c875",0},
				{PCI_CHIP_53C875J,	"53c875J",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_ATI,	{
				{PCI_CHIP_MACH32,	"Mach32",0},
				{PCI_CHIP_R200_BB,	"Radeon 8500 BB",0},
				{PCI_CHIP_MACH64CT,	"Mach64 CT",0},
				{PCI_CHIP_MACH64CX,	"Mach64 CX",0},
				{PCI_CHIP_MACH64ET,	"Mach64 ET",0},
				{PCI_CHIP_MACH64GB,	"Mach64 GB",0},
				{PCI_CHIP_MACH64GD,	"Mach64 GD",0},
				{PCI_CHIP_MACH64GI,	"Mach64 GI",0},
				{PCI_CHIP_MACH64GL,	"Mach64 GL",0},
				{PCI_CHIP_MACH64GM,	"Mach64 GM",0},
				{PCI_CHIP_MACH64GN,	"Mach64 GN",0},
				{PCI_CHIP_MACH64GO,	"Mach64 GO",0},
				{PCI_CHIP_MACH64GP,	"Mach64 GP",0},
				{PCI_CHIP_MACH64GQ,	"Mach64 GQ",0},
				{PCI_CHIP_MACH64GR,	"Mach64 GR",0},
				{PCI_CHIP_MACH64GS,	"Mach64 GS",0},
				{PCI_CHIP_MACH64GT,	"Mach64 GT",0},
				{PCI_CHIP_MACH64GU,	"Mach64 GU",0},
				{PCI_CHIP_MACH64GV,	"Mach64 GV",0},
				{PCI_CHIP_MACH64GW,	"Mach64 GW",0},
				{PCI_CHIP_MACH64GX,	"Mach64 GX",0},
				{PCI_CHIP_MACH64GY,	"Mach64 GY",0},
				{PCI_CHIP_MACH64GZ,	"Mach64 GZ",0},
				{PCI_CHIP_MACH64LB,	"Mach64 LB",0},
				{PCI_CHIP_MACH64LD,	"Mach64 LD",0},
				{PCI_CHIP_RAGE128LE,	"Rage 128 Mobility LE",0},
				{PCI_CHIP_RAGE128LF,	"Rage 128 Mobility LF",0},
				{PCI_CHIP_MACH64LG,	"Mach64 LG",0},
				{PCI_CHIP_MACH64LI,	"Mach64 LI",0},
				{PCI_CHIP_MACH64LM,	"Mach64 LM",0},
				{PCI_CHIP_MACH64LN,	"Mach64 LN",0},
				{PCI_CHIP_MACH64LP,	"Mach64 LP",0},
				{PCI_CHIP_MACH64LQ,	"Mach64 LQ",0},
				{PCI_CHIP_MACH64LR,	"Mach64 LR",0},
				{PCI_CHIP_MACH64LS,	"Mach64 LS",0},
				{PCI_CHIP_RAGE128MF,	"Rage 128 Mobility MF",0},
				{PCI_CHIP_RAGE128ML,	"Rage 128 Mobility ML",0},
				{PCI_CHIP_RAGE128PD,	"Rage 128 Pro PD",0},
				{PCI_CHIP_RAGE128PF,	"Rage 128 Pro PF",0},
				{PCI_CHIP_RAGE128PP,	"Rage 128 Pro PP",0},
				{PCI_CHIP_RAGE128PR,	"Rage 128 Pro PR",0},
				{PCI_CHIP_RAGE128TF,	"Rage 128 Pro TF",0},
				{PCI_CHIP_RAGE128TL,	"Rage 128 Pro TL",0},
				{PCI_CHIP_RAGE128TR,	"Rage 128 Pro TR",0},
				{PCI_CHIP_RADEON_QD,	"Radeon QD",0},
				{PCI_CHIP_RADEON_QE,	"Radeon QE",0},
				{PCI_CHIP_RADEON_QF,	"Radeon QF",0},
				{PCI_CHIP_RADEON_QG,	"Radeon QG",0},
				{PCI_CHIP_R200_QL,	"Radeon 8500 QL",0},
				{PCI_CHIP_R200_QN,	"Radeon 8500 QN",0},
				{PCI_CHIP_R200_QO,	"Radeon 8500 QO",0},
				{PCI_CHIP_RV200_QW,	"Radeon 7500 QW",0},
				{PCI_CHIP_RADEON_QY,	"Radeon VE QY",0},
				{PCI_CHIP_RADEON_QZ,	"Radeon VE QZ",0},
				{PCI_CHIP_R200_Ql,	"Radeon 8500 Ql",0},
				{PCI_CHIP_RADEON_LW,	"Radeon Mobility M7 LW",0},
				{PCI_CHIP_RADEON_LY,	"Radeon Mobility M6 LY",0},
				{PCI_CHIP_RADEON_LZ,	"Radeon Mobility M6 LZ",0},
				{PCI_CHIP_RAGE128SM,	"Rage 128 SM",0},
				{PCI_CHIP_RAGE128RE,	"Rage 128 RE",0},
				{PCI_CHIP_RAGE128RF,	"Rage 128 RF",0},
				{PCI_CHIP_RAGE128RK,	"Rage 128 RK",0},
				{PCI_CHIP_RAGE128RL,	"Rage 128 RL",0},
				{PCI_CHIP_MACH64VT,	"Mach64 VT",0},
				{PCI_CHIP_MACH64VU,	"Mach64 VU",0},
				{PCI_CHIP_MACH64VV,	"Mach64 VV",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_VLSI, {
				{0x0005,		"82C592-FC1",0 },
				{0x0006,		"82C593-FC1",0 },
				{0x0007,		"82C594-AFC2",0 },
				{0x0009,		"82C597-AFC2",0 },
				{0x000C,		"82C541 Lynx",0 },
				{0x000D,		"82C543 Lynx ISA",0 },
				{0x0702,	 	"VAS96011",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_AVANCE,	{
				{PCI_CHIP_ALG2301,	"ALG2301",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_NS, {
				{0x0002,		"87415",0 },
				{0xD001, 		"87410",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_TSENG, {
				{PCI_CHIP_ET4000_W32P_A, "ET4000W32P revA",0},
				{PCI_CHIP_ET4000_W32P_B, "ET4000W32P revB",0},
				{PCI_CHIP_ET4000_W32P_C, "ET4000W32P revC",0},
				{PCI_CHIP_ET4000_W32P_D, "ET4000W32P revD",0},
				{PCI_CHIP_ET6000,	 "ET6000/6100",0},
				{PCI_CHIP_ET6300,	 "ET6300",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_WEITEK, {
				{PCI_CHIP_P9000,	"P9000",0},
				{PCI_CHIP_P9100,	"P9100",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_DIGITAL, {
				{PCI_CHIP_DEC21030,	"21030/TGA",0},
				{0x0001,		"DC21050 PCI-PCI Bridge"
						 /* print_pcibridge */,0 },
				{0x0002,		"DC21040 10Mb/s Ethernet",0 },
				{0x0009,		"DC21140 10/100 Mb/s Ethernet",0 },
				{0x000D,		"TGA2",0 },
				{0x000F,		"DEFPA (FDDI PCI)",0 },
				{0x0014,		"DC21041 10Mb/s Ethernet Plus",0 },
				{0x0019,		"DC21142 10/100 Mb/s Ethernet",0 },
				{0x0021,		"DC21052",0 },
				{0x0024,		"DC21152",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CIRRUS, {
				{PCI_CHIP_GD5430,	"GD5430",0},
				{PCI_CHIP_GD5434_4,	"GD5434",0},
				{PCI_CHIP_GD5434_8,	"GD5434",0},
				{PCI_CHIP_GD5436,	"GD5436",0},
				{PCI_CHIP_GD5446,       "GD5446",0},
				{PCI_CHIP_GD5480,       "GD5480",0},
				{PCI_CHIP_GD5462,       "GD5462",0},
				{PCI_CHIP_GD5464,       "GD5464",0},
				{PCI_CHIP_GD5464BD,     "GD5464BD",0},
				{PCI_CHIP_GD5465,       "GD5465",0},
				{PCI_CHIP_GD7541,	"GD7541",0},
				{PCI_CHIP_GD7542,	"GD7542",0},
				{PCI_CHIP_GD7543,	"GD7543",0},
				{PCI_CHIP_GD7548,	"GD7548",0},
				{PCI_CHIP_GD7555,	"GD7555",0},
#ifdef VENDOR_INCLUDE_NONVIDEO
				{0x6001,		"CS4236B/CS4611 Audio" ,0},
#endif
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_IBM, {
				{0x000A,		"Fire Coral",0 },
				{0x0018,		"Token Ring",0 },
				{0x001D,		"82G2675",0 },
				{0x0022,		"82351 pci-pci bridge",0 },
				{0x00B7,		"256-bit Graphics Rasterizer",0 },
				{0x0170,		"RC1000 / GT 1000",0},
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_NCR_2,	{
				{0x0000,		NULL,0}}},
#endif
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_WD, {
				{0x3296,		"WD 7197",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_AMD, {
				{0x2000,		"79C970 Lance",0 },
				{0x2020,		"53C974 SCSI",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_TRIDENT, {
				{PCI_CHIP_9320,		"TGUI 9320",0},
				{PCI_CHIP_9420,		"TGUI 9420",0},
				{PCI_CHIP_9440,		"TGUI 9440",0},
				{PCI_CHIP_9660,		"TGUI 96xx",0},
				{PCI_CHIP_9388,		"Cyber 9388",0},
				{PCI_CHIP_9397,		"Cyber 9397",0},
				{PCI_CHIP_939A,		"Cyber 939A/DVD",0},
				{PCI_CHIP_9520,		"Cyber 9520",0},
				{PCI_CHIP_9525,		"Cyber 9525/DVD",0},
				{PCI_CHIP_9540,		"Cyber 9540",0},
				{PCI_CHIP_9750,		"3DImage975",0},
				{PCI_CHIP_9850,		"3DImage985",0},
				{PCI_CHIP_9880,		"Blade3D",0},
				{PCI_CHIP_9910,		"Cyber/BladeXP",0},
				{PCI_CHIP_9930,		"CyberBlade/XPm",0},
				{PCI_CHIP_8400,		"CyberBlade/i7",0},
				{PCI_CHIP_8420,		"CyberBlade/DSTN/i7",0},
				{PCI_CHIP_8500,		"CyberBlade/i1",0},
				{PCI_CHIP_8520,		"CyberBlade/DSTN/i1",0},
				{PCI_CHIP_8600,		"CyberBlade/Ai1",0},
				{PCI_CHIP_8620,		"CyberBlade/DSTN/Ai1",0},
				{PCI_CHIP_8800,		"CyberBlade/XP/Ai1",0},
				{PCI_CHIP_8820,		"CyberBlade/XP/DSTN/Ai1",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
     {PCI_VENDOR_ALI, {
				{0x1435,		"M1435",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_MATROX, {
				{PCI_CHIP_MGA2085,	"MGA 2085PX",0},
				{PCI_CHIP_MGA2064,	"MGA 2064W",0},
				{PCI_CHIP_MGA1064,	"MGA 1064SG",0},
				{PCI_CHIP_MGA2164,	"MGA 2164W",0},
				{PCI_CHIP_MGA2164_AGP,	"MGA 2164W AGP",0},
				{PCI_CHIP_MGAG200_PCI,	"MGA G200 PCI",0},
				{PCI_CHIP_MGAG200,	"MGA G200 AGP",0},
				{PCI_CHIP_MGAG400,	"MGA G400 AGP",0},
				{PCI_CHIP_MGAG550,	"MGA G550 AGP",0},
				{PCI_CHIP_MGAG100_PCI,	"MGA G100 PCI",0},
				{PCI_CHIP_MGAG100,	"MGA G100 AGP",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CHIPSTECH, {
				{PCI_CHIP_65545,	"65545",0},
				{PCI_CHIP_65548,	"65548",0},
				{PCI_CHIP_65550,	"65550",0},
				{PCI_CHIP_65554,	"65554",0},
				{PCI_CHIP_65555,	"65555",0},
				{PCI_CHIP_68554,	"68554",0},
				{PCI_CHIP_69000,	"69000",0},
				{PCI_CHIP_69030,	"69030",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_MIRO, {
				{0x5601,		"ZR36050",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_NEC, {
				{0x0046,		"PowerVR PCX2",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_FD, {
				{0x0000,		"TMC-18C30 (36C70)",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_SIS,	{
				{PCI_CHIP_SG86C201,	"SG86C201",0},
				{PCI_CHIP_SG86C202,	"SG86C202",0},
				{PCI_CHIP_SG86C205,	"SG86C205",0},
				{PCI_CHIP_SG86C215,	"SG86C215",0},
				{PCI_CHIP_SG86C225,	"SG86C225",0},
				{PCI_CHIP_SIS5597,	"5597",0},
				{PCI_CHIP_SIS530,	"530",0},
				{PCI_CHIP_SIS6326,	"6326",0},
				{PCI_CHIP_SIS300,	"300",0},
				{PCI_CHIP_SIS630,	"630",0},
				{PCI_CHIP_SIS540,	"540",0},
				{PCI_CHIP_SIS82C204,    "82C204",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
     {PCI_VENDOR_HP, {
				{0x1030,		"J2585A",0 },
				{0x1031,		"J2585B",0 },
				{0x0000,		NULL,0}}},
     {PCI_VENDOR_SMC_PCTECH, {
				{0x1000,		"FDC 37C665/RZ1000",0 },
				{0x1001,		"FDC /RZ1001",0 },
				{0x0000,		NULL,0}}},
     {PCI_VENDOR_DPT, {
				{0xA400,		"SmartCache/Raid",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_SGS,	{
				{PCI_CHIP_STG2000,	"STG2000",0},
				{PCI_CHIP_STG1764,	"STG1764",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_BUSLOGIC, {
				{PCI_CHIP_946C_01,	"946C 01",0},
				{PCI_CHIP_946C_10,	"946C 10",0},
				{PCI_CHIP_FLASH_POINT,	"FlashPoint",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_TI,	{
				{PCI_CHIP_TI_PERMEDIA,	"Permedia",0},
				{PCI_CHIP_TI_PERMEDIA2,	"Permedia 2",0},
				{PCI_CHIP_PCI_1130,	"PCI 1130",0},
				{PCI_CHIP_PCI_1131,	"PCI 1131",0},
				{0x8019,		"TSB12LV23 IEEE1394/FireWire",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
	{PCI_VENDOR_SONY, {
				{0x8009,		"CXD1947A IEEE1394/Firewire",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_OAK, {
				{PCI_CHIP_OTI107,	"OTI107",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_WINBOND, {
				{PCI_CHIP_89C940,	"89C940 NE2000-PCI",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_MOTOROLA, {
				{PCI_CHIP_MPC105_EAGLE,	"MPC105 Eagle",0},
				{PCI_CHIP_MPC105_GRACKLE,"MPC105 Grackle",0},
				{PCI_CHIP_RAVEN,	"Raven",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_OAK, {
				{PCI_CHIP_ULTRA_DMA,	"IDE UltraDMA/33",0},
				{PCI_CHIP_DC5030,	"DC5030",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_NUMNINE, {
				{PCI_CHIP_I128,		"Imagine 128",0},
				{PCI_CHIP_I128_2,	"Imagine 128 II",0},
				{PCI_CHIP_I128_T2R,	"Imagine 128 Rev 3D T2R",0},
				{PCI_CHIP_I128_T2R4,	"Imagine 128 Rev IV T2R4",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_UMC,	{
                                {0x0101,		"UM8673F",0},
                                {0x673A,		"UM8886BF",0},
                                {0x886A,		"UM8886A",0},
                                {0x8881,		"UM8881F",0},
                                {0x8886,		"UM8886F",0},
                                {0x8891,		"UM8891A",0},
                                {0x9017,		"UM9017F",0},
                                {0xE886,		"UM8886N",0},
                                {0xE891,		"UM8891N",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_X, {
                                {0x0001,		"ITT AGX016",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_PICOP, {
                                {0x0001,		"PT86C52x Vesuvius",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_MYLEX, {
                                {0x0010,		"RAID Controller",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_APPLE, {
                                {0x0001,		"Bandit",0 },
				{0x0002,		"Grand Central",0 },
				{0x000E,		"Hydra",0 },
				{0x0019,		"Keylargo USB",0 },
				{0x0020,		"Uni-North AGP",0 },
				{0x0022,		"Keylargo I/O",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_NEXGEN, {
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_QLOGIC, {
                                {0x1020,		"ISP1020",0 },
				{0x1022,		"ISP1022",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_CYRIX, {
                                {0x0000,		"5510",0 },
				{0x0001,		"PCI Master",0 },
				{0x0002,		"5520",0 },
				{0x0100,		"5530 Kahlua Legacy",0 },
				{0x0101,		"5530 Kahlua SMI",0 },
				{0x0102,		"5530 Kahlua IDE",0 },
				{0x0103,		"5530 Kahlua Audio",0 },
				{0x0104,		"5530 Kahlua Video",0 },
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_LEADTEK, {
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_CONTAQ, {
                                {0x0600,		"82C599",0 },
                                {0xc693,		"82C693",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_FOREX, {
				{0x0000,		NULL,0}}},
#endif
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_SBS, {
				{0x0040,		"dataBLIZZARD",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_OLICOM, {
                                {0x0001,		"OC-3136",0 },
				{0x0011,		"OC-2315",0 },
				{0x0012,		"OC-2325",0 },
				{0x0013,		"OC-2183",0 },
				{0x0014,		"OC-2326",0 },
				{0x0021,		"OC-6151",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_SUN, {
				{PCI_CHIP_EBUS,		"EBUS",0 },
				{PCI_CHIP_HAPPY_MEAL,	"Happy Meal",0 },
				{PCI_CHIP_SIMBA,	"Simba Advanced PCI bridge",0 },
				{PCI_CHIP_PSYCHO,	"U2P PCI host bridge (Psycho)",0 },
				{PCI_CHIP_SCHIZO,	"PCI host bridge (Schizo)",0 },
				{PCI_CHIP_SABRE,	"UltraSPARC IIi host bridge (Sabre)",0 },
				{PCI_CHIP_HUMMINGBIRD,	"UltraSPARC IIi host bridge (Hummingbird)",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CMD, {
                                {0x0640,		"640A",0 },
				{0x0643,		"643",0 },
				{0x0646,		"646",0 },
				{0x0670,		"670",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_APPIAN, {
                                {0x3D32,		"Jeronimo 2000 AGP",0 },
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_VISION, {
                                {0x0001,		"QD 8500",0 },
				{0x0002,		"QD 8580",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_BROOKTREE,	{
				{PCI_CHIP_BT848,	"848",0},
				{PCI_CHIP_BT849,	"849",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_SIERRA, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ACC, {
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_WINBOND_2, {
                                {0x0001,		"W83769F",0 },
                                {0x0105,		"SL82C105",0 },
                                {0x0565,		"W83C553",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_DATABOOK, {
                                {0xB106, "DB87144",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_3COM, {
                                {0x5900, "3C590 10bT",0 },
                                {0x5950, "3C595 100bTX",0 },
                                {0x5951, "3C595 100bT4",0 },
                                {0x5952, "3C595 10b-MII",0 },
                                {0x9000, "3C900 10bTPO",0 },
                                {0x9001, "3C900 10b Combo",0 },
				/* Is it OK for 2 devices to have the same name ? */
                                {0x9005, "3C900 10b Combo",0 },
                                {0x9050, "3C905 100bTX",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_SMC, {
                                {0x0005, "9432 TX",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ALI_2, {
                                {0x1445, "M1445",0 },
                                {0x1449, "M1449",0 },
                                {0x1451, "M1451",0 },
                                {0x1461, "M1461",0 },
                                {0x1489, "M1489",0 },
                                {0x1511, "M1511",0 },
                                {0x1513, "M1513",0 },
                                {0x1521, "M1521",0 },
                                {0x1523, "M1523",0 },
                                {0x1531, "M1531 Aladdin IV",0 },
                                {0x1533, "M1533 Aladdin IV",0 },
                                {0x5215, "M4803",0 },
                                {0x5219, "M5219",0 },
                                {0x5229, "M5229 TXpro",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_MITSUBISHI, {
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_SURECOM, {
                                {0x0E34, "NE-34PCI Lan",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_NEOMAGIC,	{
				{PCI_CHIP_NM2070,	"NM2070",0},
				{PCI_CHIP_NM2090,	"NM2090",0},
				{PCI_CHIP_NM2093,	"NM2093",0},
				{PCI_CHIP_NM2160,	"NM2160",0},
				{PCI_CHIP_NM2200,	"NM2200",0},
				{PCI_CHIP_NM2230,	"NM2230 MagicMedia 256AV+",0},
				{PCI_CHIP_NM2360,	"NM2360",0},
				{PCI_CHIP_NM2380,	"NM2380",0},
#ifdef VENDOR_INCLUDE_NONVIDEO
				{0x8005,			"NM2360 MagicMedia 256ZX Audio",0},
#endif
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_ASP, {
                                { 0x1200, "ABP940",0 },
                                { 0x1300, "ABP940U",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CERN, {
                                { 0x0001, "STAR/RD24 SCI-PCI (PMC)",0 },
                                { 0x0002, "STAR/RD24 SCI-PCI (PMC)",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_NVIDIA,	{
				{PCI_CHIP_NV1,		"NV1",0},
				{PCI_CHIP_DAC64,	"DAC64",0},
				{PCI_CHIP_TNT,		"RIVA TNT",0},
				{PCI_CHIP_TNT2,		"RIVA TNT2/TNT2 Pro",0},
				{PCI_CHIP_UTNT2,	"RIVA TNT2 Ultra",0},
				{PCI_CHIP_VTNT2,	"Vanta",0},
				{PCI_CHIP_UVTNT2,	"Riva TNT2 M64",0},
				{PCI_CHIP_ITNT2,	"Aladdin TNT2",0},
				{PCI_CHIP_GEFORCE256,	"GeForce 256",0},
				{PCI_CHIP_GEFORCEDDR,	"GeForce DDR",0},
				{PCI_CHIP_QUADRO,	"Quadro",0},
				{PCI_CHIP_GEFORCE2MX,	"GeForce2 MX/MX 400",0},
				{PCI_CHIP_GEFORCE2MXDDR,"GeForce2 MX 100/200",0},
				{PCI_CHIP_GEFORCE2GO,   "GeForce2 Go", 0},
				{PCI_CHIP_QUADRO2MXR,	"Quadro2 MXR",0},
				{PCI_CHIP_GEFORCE2GTS,	"GeForce2 GTS/Pro",0},
				{PCI_CHIP_GEFORCE2GTS_1,"GeForce2 Ti",0},
				{PCI_CHIP_GEFORCE2ULTRA,"GeForce2 Ultra",0},
				{PCI_CHIP_QUADRO2PRO,	"Quadro2 Pro",0},
				{PCI_CHIP_0x0170,	"0x0170",0},
				{PCI_CHIP_0x0171,	"0x0171",0},
				{PCI_CHIP_0x0172,	"0x0172",0},
				{PCI_CHIP_0x0173,	"0x0173",0},
				{PCI_CHIP_0x0174,	"0x0174",0},
				{PCI_CHIP_0x0175,	"0x0175",0},
				{PCI_CHIP_0x0178,	"0x0178",0},
				{PCI_CHIP_0x017A,	"0x017A",0},
				{PCI_CHIP_0x017B,	"0x017B",0},
				{PCI_CHIP_0x017C,	"0x017C",0},
				{PCI_CHIP_IGEFORCE2,	"GeForce2 Integrated",0},
				{PCI_CHIP_GEFORCE3,	"GeForce3",0},
				{PCI_CHIP_GEFORCE3_1,	"GeForce3 Ti 200",0},
				{PCI_CHIP_GEFORCE3_2,	"GeForce3 Ti 500",0},
				{PCI_CHIP_QUADRO_DDC,	"Quadro DDC",0},
				{PCI_CHIP_0x0250,	"0x0250",0},
				{PCI_CHIP_0x0258,	"0x0258",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_IMS, {
				{PCI_CHIP_IMSTT128,	"TwinTurbo 128", 0},
				{PCI_CHIP_IMSTT3D,	"TwinTurbo 3D", 0},
#ifdef VENDOR_INCLUDE_NONVIDEO
                                {0x8849, "8849",0 },
#endif
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_TEKRAM, {
                                {0x690C, "DC690C",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_TUNDRA, {
                                {0x0000, "CA91C042 Universe",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_AMCC, {
                                {0x8043, "Myrinet PCI (M2-PCI-32)",0 },
                                {0x807D, "S5933 PCI44",0 },
                                {0x809C, "S5933 Traquair HEPC3",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_INTEGRAPHICS, {
                                {0x1680, "IGA-1680",0 },
                                {0x1682, "IGA-1682",0 },
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_REALTEC, {
                                {0x8029, "8029",0 },
                                {0x8129, "8129",0 },
                                {0x8139, "RTL8139 10/100 Ethernet",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_TRUEVISION, {
                                {0x000C, "Targa 1000",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_INITIO, {
                                {0x9100, "320 P",0 },
				{0x0000,		NULL,0}}},
	{PCI_VENDOR_SIGMADESIGNS_2, {
	            {0x8300, "EM8300 MPEG2 decoder", 0 },
	            {0x0000, NULL,0}}},
    {PCI_VENDOR_VIA, {
				{0x0501, "VT 8501 MVP4 Host Bridge",0 },
                                {0x0505, "VT 82C505",0 },
                                {0x0561, "VT 82C505",0 },
                                {0x0571, "VT 82C586 MVP3 IDE Bridge",0 },
                                {0x0576, "VT 82C576 3V",0 },
                                {0x0586, "VT 82C586 MVP3 ISA Bridge",0 },
                                {0x0686, "VT 82C686 MVP4 ISA Bridge",0 },
                                {0x0597, "VT 82C598 MVP3 Host Bridge",0 },
                                {0x3038, "VT 82C586 MVP3 USB Controller",0 },
                                {0x3040, "VT 82C586B MVP3 ACPI Bridge",0 },
				{0x3057, "VT 8501 MVP4 ACPI Bridge",0 },
				{0x3058, "VT 8501 MVP4 MultiMedia",0 },
				{0x3068, "VT 8501 MVP4 Modem",0 },
				{0x8501, "VT 8501 MVP4 PCI/AGP Bridge",0 },
                                {0x8598, "VT 82C598 MVP3 PCI/AGP Bridge",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_VORTEX, {
                                {0x0001, "GDT 6000b",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_EF, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_FORE, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_IMAGTEC, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_PLX, {
				{0x0000,		NULL,0}}},
#endif
#endif
    {PCI_VENDOR_NVIDIA_SGS,	{
				{PCI_CHIP_RIVA128,	"Riva128",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ALLIANCE, {
				{PCI_CHIP_AP6410,	"ProMotion 6410",0},
				{PCI_CHIP_AP6422,	"ProMotion 6422",0},
				{PCI_CHIP_AT24,		"ProMotion AT24",0},
				{PCI_CHIP_AT3D,		"ProMotion AT3D",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_VMIC, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_DIGI, {
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_MUTECH, {
                                {0x0001,		 "MV1000",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_RENDITION,	{
				{PCI_CHIP_V1000,	"Verite 1000",0},
				{PCI_CHIP_V2x00,	"Verite 2100/2200",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_TOSHIBA, {
				{0x0000,		NULL,0}}},
	{ PCI_VENDOR_RICOH, {
				{ 0x0475, 	"RL5C475 PCI-CardBus bridge/PCMCIA",0 },
                { 0x0000,		NULL,0}}},						
    {PCI_VENDOR_ZEINET, {
                                {0x0001, "1221",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_LITEON, {
                                {0x0002, "82C168/9 PNIC 10/100BaseTX",0 },
                                {0xC115, "LC82C115 PNIC II 10/100BaseTX",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_SPECIALIX, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CONTROL, {
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_CYCLADES, {
				{0x0000,		NULL,0}}},
#endif
#endif
    {PCI_VENDOR_3DFX, {
				{PCI_CHIP_VOODOO_GRAPHICS, "Voodoo Graphics",0},
				{PCI_CHIP_VOODOO2, 	"Voodoo2",0},
				{PCI_CHIP_BANSHEE, 	"Banshee",0},
				{PCI_CHIP_VOODOO3, 	"Voodoo3",0},
				{PCI_CHIP_VOODOO5, 	"Voodoo5",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_SIGMADESIGNS, {
                                {0x6401, "REALmagic64/GX (SD 6425)",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ENSONIQ, {
                                {0x5000, "es1370 (AudioPCI)",0 },
                                {0x1371, "es1371",0 },
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
	{PCI_VENDOR_ROCKWELL, {
				{0x2005,	"RS56/SP-PCI11P1 56K V90 modem/spkrphone",0 },
				{0x0000,		NULL,0}}},
#ifdef INCLUDE_EMPTY_LISTS
    {PCI_VENDOR_YOKOGAWA, {
				{0x0000,		NULL,0}}},
#endif
#endif
    {PCI_VENDOR_TRITECH,	{
				{PCI_CHIP_TR25202,	"Pyramid3D TR25202",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_NVIDIA_SGS, {
                                {0x0018, "Riva128",0 },
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_SYMPHONY, {
                                {0x0001, "82C101",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_TEKRAM_2, {
                                {0xDC29, "DC290",0 },
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_3DLABS, {
				{PCI_CHIP_300SX,	"GLINT 300SX",0},
				{PCI_CHIP_500TX,	"GLINT 500TX",0},
				{PCI_CHIP_DELTA,	"GLINT Delta",0},
				{PCI_CHIP_PERMEDIA,	"GLINT Permedia",0},
				{PCI_CHIP_MX,		"GLINT MX",0},
				{PCI_CHIP_PERMEDIA2,	"GLINT Permedia 2",0},
				{PCI_CHIP_GAMMA,	"GLINT Gamma",0},
				{PCI_CHIP_PERMEDIA2V,	"GLINT Permedia 2v",0},
				{PCI_CHIP_PERMEDIA3,	"GLINT Permedia 3",0},
				{PCI_CHIP_PERMEDIA4,	"GLINT Permedia 4",0},
				{PCI_CHIP_R4,		"GLINT R4",0},
				{PCI_CHIP_R4ALT,	"GLINT R4 (Alt)",0},
				{PCI_CHIP_GAMMA2,	"GLINT Gamma 2",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_AVANCE_2, {
				{PCI_CHIP_ALG2064,	"ALG2064",0},
				{PCI_CHIP_ALG2501,	"ALG2501",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_S3,	{
				{PCI_CHIP_PLATO,	"PLATO/PX",0},
				{PCI_CHIP_VIRGE,	"ViRGE",0},
				{PCI_CHIP_TRIO,		"Trio32/64",0},
				{PCI_CHIP_AURORA64VP,	"Aurora64V+",0},
				{PCI_CHIP_TRIO64UVP,	"Trio64UV+",0},
				{PCI_CHIP_TRIO64V2_DXGX,"Trio64V2/DX or /GX",0},
				{PCI_CHIP_PLATO_PX,	"PLATO/PX",0},
				{PCI_CHIP_Trio3D,	"Trio3D",0},
				{PCI_CHIP_Trio3D_2X,	"Trio3D/2X",0},
				{PCI_CHIP_VIRGE_VX,	"ViRGE/VX",0},
				{PCI_CHIP_VIRGE_DXGX,	"ViRGE/DX or /GX",0},
				{PCI_CHIP_VIRGE_GX2,	"ViRGE/GX2",0},
				{PCI_CHIP_SAVAGE3D,	"Savage3D (86E391)",0},
				{PCI_CHIP_SAVAGE3D_MV,	"Savage3D+MacroVision (86E390)",0},
				{PCI_CHIP_SAVAGE4,	"Savage4",0},
				{PCI_CHIP_SAVAGE2000,	"Savage2000",0},
				{PCI_CHIP_SAVAGE_MX,	"Savage/MX",0},
				{PCI_CHIP_SAVAGE_MX_MV,	"Savage/MX-MV",0},
				{PCI_CHIP_SAVAGE_IX,	"Savage/IX",0},
				{PCI_CHIP_SAVAGE_IX_MV,	"Savage/IX-MV",0},
				{PCI_CHIP_PROSAVAGE_PM,	"ProSavage PM133",0},
				{PCI_CHIP_PROSAVAGE_KM,	"ProSavage KM133",0},
				{PCI_CHIP_VIRGE_MX,	"ViRGE/MX",0},
				{PCI_CHIP_VIRGE_MXPLUS,	"ViRGE/MX+",0},
				{PCI_CHIP_VIRGE_MXP,	"ViRGE/MX+MV",0},
				{PCI_CHIP_868,		"868",0},
				{PCI_CHIP_928,		"928",0},
				{PCI_CHIP_864_0,	"864",0},
				{PCI_CHIP_864_1,	"864",0},
				{PCI_CHIP_964_0,	"964",0},
				{PCI_CHIP_964_1,	"964",0},
				{PCI_CHIP_968,		"968",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_INTEL,{
#ifdef VENDOR_INCLUDE_NONVIDEO
                                {0x0482, "82375EB pci-eisa bridge",0},
				{0x0483, "82424ZX cache dram controller",0},
				{0x0484, "82378IB/ZB pci-isa bridge",0x0601},
				{0x0486, "82430ZX Aries",0},
				{0x04A3, "82434LX/NX pci cache mem controller",0},
				{0x0960, "960RD processor/bridge",0},
				{0x1221, "82092AA",0},
				{0x1222, "82092AA",0},
				{0x1223, "SAA7116",0},
				{0x1226, "82596",0},
				{0x1227, "82865",0},
				{0x1229, "82557/8/9 10/100MBit network controller",0 },
				{0x122D, "82437 Triton",0},
				{0x122E, "82471 Triton",0},
				{0x1230, "82371 bus-master IDE controller",0},
				{0x1234, "82371MX bus-master IDE controller",0},
				{0x1235, "82437MX",0},
				{0x1237, "82441FX Natoma",0},
				{0x124B, "82380FB",0},
				{0x1250, "82439",0},
				{0x7000, "82371 pci-isa bridge",0},
				{0x7010, "82371 bus-master IDE controller",0},
				{0x7020, "82371 bus-master IDE controller",0},
				{0x7030, "82437VX",0},
				{0x7100, "82439TX",0},
				{0x7110, "82371AB PIIX4 ISA",0},
				{0x7111, "82371AB PIIX4 IDE",0},
				{0x7112, "82371AB PIIX4 USB",0},
				{0x7113, "82371AB PIIX4 ACPI",0},
				{0x7180, "82443LX PAC Host",0},
				{0x7181, "82443LX PAC AGP",0},
				{0x7190, "82443BX Host",0},
				{0x7191, "82443BX AGP",0},
				{0x7192, "82443BX Host (no AGP)",0},
				{0x71a0, "82443GX Host",0},
				{0x71a1, "82443GX AGP",0},
				{0x71a2, "82443GX Host (no AGP)",0},
				{0x84C4, "P6",0},
				{0x84C5, "82450GX20",0},
				{PCI_CHIP_I810_BRIDGE,	"i810 Bridge",0},
				{PCI_CHIP_I810_DC100_BRIDGE,	"i810-dc100 Bridge",0},
				{PCI_CHIP_I810_E_BRIDGE,"i810e Bridge",0},
				{PCI_CHIP_I815_BRIDGE,	"i815 Bridge",0},
				{PCI_CHIP_I830_M_BRIDGE,"i830M Bridge",0},
#endif
				{PCI_CHIP_I740_AGP,	"i740 (AGP)",0},
				{PCI_CHIP_I810,		"i810",0},
				{PCI_CHIP_I810_DC100,	"i810-dc100",0},
				{PCI_CHIP_I810_E,	"i810e",0},
				{PCI_CHIP_I815,		"i815",0},
				{PCI_CHIP_I830_M,	"i830M",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ADAPTEC, {
				{0x0010, "2940U2",0 },
				{0x1078, "7810",0 },
				{0x5078, "7850",0 },
				{0x5578, "7855",0 },
				{0x6078, "7860",0 },
				{0x6178, "2940AU",0 },
				{0x7078, "7870",0 },
				{0x7178, "2940",0 },
				{0x7278, "7872",0 },
				{0x7378, "398X",0 },
				{0x7478, "2944",0 },
				{0x7895, "7895",0 },
				{0x8078, "7880",0 },
				{0x8178, "2940U/UW",0 },
				{0x8278, "3940U/UW",0 },
				{0x8378, "389XU",0 },
				{0x8478, "2944U",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ADAPTEC_2, {
                                {0x001F, "7890/7891",0 },
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ATRONICS, {
                                {0x2015, "IDE-2015PL",0 },
 				{0x0000,		NULL,0}}},
    {PCI_VENDOR_ARK, {
				{PCI_CHIP_1000PV,	"1000PV",0},
				{PCI_CHIP_2000PV,	"2000PV",0},
				{PCI_CHIP_2000MT,	"2000MT",0},
				{PCI_CHIP_2000MI,	"2000MI",0},
				{0x0000,		NULL,0}}},
#ifdef VENDOR_INCLUDE_NONVIDEO
    {PCI_VENDOR_YAMAHA, {
				{0x000a,	        "YMF740-V Audio",0},
				{0x0000,		NULL,0}}},
#endif
    {PCI_VENDOR_SMI, {
				{PCI_CHIP_SMI910,	"Lynx",0},
				{PCI_CHIP_SMI810,	"LynxE",0},
				{PCI_CHIP_SMI820,	"Lynx3D",0},
				{PCI_CHIP_SMI710,	"LynxEM",0},
				{PCI_CHIP_SMI712,	"LynxEM+",0},
				{PCI_CHIP_SMI720,	"Lynx3DM",0},
				{0x0000,		NULL,0}}},
    {PCI_VENDOR_VMWARE, {
    				{PCI_CHIP_VMWARE0405,	"PCI SVGA (FIFO)",0},
    				{PCI_CHIP_VMWARE0710,	"LEGACY SVGA",0},
				{0x0000,		NULL,0}}},
    {0x0000, {
				{0x0000,		NULL,0}}},
};
#endif

#ifdef DECLARE_CARD_DATASTRUCTURES

/* Increase this as required */
#define MAX_CARD_PER_VENDOR 64

typedef void (*pciPrintProcPtr)(pciCfgRegs *);
typedef struct {
    unsigned short VendorID;
    struct pciCard {
	unsigned short SubsystemID;
        char *CardName;
	CARD16 class;
	pciPrintProcPtr printFunc;
    } Device[MAX_CARD_PER_VENDOR];
} pciVendorCardInfo;

extern pciVendorCardInfo* xf86PCICardInfo;

#ifdef INIT_PCI_CARD_INFO

#define NF (pciPrintProcPtr)NULL

static pciVendorCardInfo xf86PCICardInfoData[] = {
#ifdef VENDOR_INCLUDE_NONVIDEO
	{ PCI_VENDOR_3COM, {
                        { 0x9005, "PCI Combo ethernet card",0,NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#endif
#ifdef VENDOR_INCLUDE_NONVIDEO
	{ PCI_VENDOR_ADAPTEC, {
                        { 0x7881, "AHA-2940U/UW SCSI",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#endif
	/* ATI card info deleted;  unmaintainable */
#ifdef VENDOR_INCLUDE_NONVIDEO
	{ PCI_VENDOR_COMPAQ, {
                        { 0xC001, "NC3121",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_NCR_1, {
	                { 0x1000, "SCSI HBA",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
    { PCI_VENDOR_REALTEC, {
                        { 0x8139, "Generic",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_CREATIVE_2, {
			{ 0x1017, "3D Blaster Banshee",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{PCI_VENDOR_DIGITAL, {
			{ 0x500A, "EtherWORKS 10/100",0, NF},
                        { 0x0000, (char *)NULL,0, NF } } },
#endif
	{ PCI_VENDOR_SONY, {
			{ 0x8051, "Vaio Video",0,NF },
#ifdef VENDOR_INCLUDE_NONVIDEO
			{ 0x8052, "Vaio Audio",0,NF },
			{ 0x8054, "Vaio Firewire",0,NF },
			{ 0x8056, "Vaio Modem",0,NF },
			{ 0x8057, "Vaio Ethernet",0,NF },
#endif
			{ 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_DIAMOND, {
                        { 0x0003, "Monster Fusion",0, NF },
			{ 0x00b8, "Fire GL1",0, NF },
                        { 0x0100, "Stealth II G460",0, NF },
                        { 0x0154, "Fire GL 1000 PRO",0, NF },
			{ 0x0172, "Fire GL2",0, NF },
			{ 0x0173, "Fire GL2",0, NF },
                        { 0x0550, "Viper 550",0, NF },
                        { 0x1092, "Viper 330",0, NF },
                        { 0x1103, "Fire GL 1000",0, NF },
                        { 0x2000, "Stealth II S220",0, NF },
			{ 0x2110, "Sonic Impact S70",0, NF },
                        { 0x4803, "Monster Fusion",0, NF },
                        { 0x6820, "Viper 770",0, NF },
                        { 0x8000, "C&T 69000",0, NF },
			{ 0x8760, "Fireport 40 Dual",0, NF },
                        { 0x8a10, "Stealth 3D 4000",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_APPIAN, {
                        { 0x3d32, "Jeronimo 2000",0, NF },
                        { 0x3db3, "Jeronimo Pro",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_3DFX, {
                        { PCI_CARD_VOODOO3_2000, "Voodoo3 2000",0, NF },
                        { PCI_CARD_VOODOO3_3000, "Voodoo3 3000",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_3DLABS, {
                        { 0x0096, "Permedia",0, NF },
                        { 0x0098, "PermediaNT",0, NF },
                        { 0x0099, "PermediaLC",0, NF },
                        { 0x0100, "Permedia2 PCI",0, NF },
                        { 0x0101, "Permedia2 AGP",0, NF },
                        { 0x0102, "Oxygen GMX2000 PCI",0, NF },
                        { 0x0106, "Oxygen GMX2000 AGP",0, NF },
                        { 0x0116, "Oxygen GVX1 AGP",0, NF },
                        { 0x0121, "Oxygen VX1 PCI",0, NF },
                        { 0x0122, "Oxygen ACX AGP",0, NF },
                        { 0x0123, "Oxygen ACX PCI",0, NF },
                        { 0x0125, "Oxygen VX1 AGP",0, NF },
                        { 0x0127, "Permedia3 Create!",0, NF },
                        { 0x0134, "Oxygen GVX1 PCI",0, NF },
                        { 0x0136, "Oxygen GVX210 AGP",0, NF },
			{ 0x0139, "Oxygen GVX1 Pro AGP",0, NF },
                        { 0x0140, "Oxygen VX1-16 AGP",0, NF },
                        { 0x0144, "Oxygen VX1-4X AGP",0, NF },
                        { 0x0400, "Oxygen GVX420 AGP",0, NF },
                        { 0x0800, "Oxygen VX1-1600SW PCI",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_ELSA, {
                        { 0x0914, "Winner 1000",0, NF },
                        { 0x0930, "Winner 1000PRO 864",0, NF },
                        { 0x0931, "Winner 1000PRO Trio32",0, NF },
                        { 0x0932, "Winner 1000Trio Trio64",0, NF },
                        { 0x0933, "Winner 1000TrioV Trio64V+",0, NF },
                        { 0x0934, "Victory 3D",0, NF },
                        { 0x0935, "Winner 1000 T2D",0, NF },
                        { 0x0936, "Winner 1000PRO 868",0, NF },
                        { 0x0937, "Winner 1000PRO/X 868",0, NF },
                        { 0x0938, "Winner 1000ViRGE",0, NF },
                        { 0x0939, "Winner 1000ViRGE/DX",0, NF },
                        { 0x093a, "Winner 1000/T2DX",0, NF },
                        { 0x093b, "Winner DUO M5",0, NF },
                        { 0x093c, "Victory 1000",0, NF },
                        { 0x0940, "Winner 2000PRO 964/TVP3020",0, NF },
                        { 0x0941, "Winner 2000PRO/X 968/TVP3020",0, NF },
                        { 0x0942, "Winner 2000PRO/X 968/TVP3026",0, NF },
                        { 0x0943, "Winner 2000AVI 968/TVP3026",0, NF },
                        { 0x0948, "Winner 2000PRO-8 964/RGB528",0, NF },
                        { 0x094a, "Winner 2000PRO-8 968/RGB528",0, NF },
                        { 0x094b, "Winner 2000PRO-8 968/TVP3030",0, NF },
                        { 0x0950, "ViRGE/VX",0, NF },
                        { 0x0951, "Winner 2000AVI 3D",0, NF },
                        { 0x0952, "Winner 2000AVI 220",0, NF },
                        { 0x0960, "Winner 3000M",0, NF },
                        { 0x0962, "Winner 3000L",0, NF },
                        { 0x0964, "Winner 3000XL",0, NF },
                        { 0x096a, "Winner 3000Twin",0, NF },
                        { 0x096c, "Winner 3000LT",0, NF },
                        { 0x0980, "GLoria 4 TVP3026",0, NF },
                        { 0x0982, "GLoria 4 TVP3030",0, NF },
                        { 0x0981, "GLoria 8",0, NF },
                        { 0x0a10, "GLoria M",0, NF },
                        { 0x0a14, "GLoria S",0, NF },
                        { 0x0a31, "Winner 2000 Office",0, NF },
                        { 0x0a32, "GLoria Synergy P2C",0, NF },
                        { 0x0a33, "GLoria Synergy P2C",0, NF },
                        { 0x0a34, "GLoria Synergy P2V",0, NF },
                        { 0x0a35, "GLoria Synergy P2A",0, NF },
                        { 0x0a36, "Quad GLoria Synergy P2A",0, NF },
                        { 0x0a40, "GLoria MX",0, NF },
                        { 0x0a41, "GLoria XL",0, NF },
                        { 0x0a42, "GLoria XXL",0, NF },
                        { 0x0a43, "Winner 2000 Office P2V",0, NF },
                        { 0x0a44, "Winner 2000 Office P2A",0, NF },
                        { 0x0a80, "GLoria S MAC",0, NF },
                        { 0x0c10, "Victory Erazor 4",0, NF },
                        { 0x0c11, "Victory Erazor 8",0, NF },
                        { 0x0c12, "Winner 1000 R3D",0, NF },
                        { 0x0c13, "Winner 1000 ZX4",0, NF },
                        { 0x0c14, "Victory Erazor/LT SGRAM",0, NF },
                        { 0x0c15, "Victory Erazor/LT SDRAM",0, NF },
                        { 0x0c18, "Erazor II SGRAM",0, NF },
                        { 0x0c19, "Erazor II SDRAM video",0, NF },
                        { 0x0c1a, "Synergy Pro",0, NF },
                        { 0x0c1c, "Erazor II SDRAM",0, NF },
                        { 0x0c20, "Synergy II 32",0, NF },
                        { 0x0c21, "Synergy II 16",0, NF },
                        { 0x0c22, "Erazor III",0, NF },
                        { 0x0c23, "Erazor III video",0, NF },
                        { 0x0d10, "Victory II SGRAM",0, NF },
                        { 0x0d11, "Victory II SDRAM",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_HERCULES, {
                        { 0x0001, "Thriller3D",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_IBM, {
                        { 0x00ba, "Thinkpad 600 NeoMagic NM2160",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{PCI_VENDOR_INTEL, {
#ifdef VENDOR_INCLUDE_NONVIDEO
                        { 0x0009, "PCI 10/100Mb/s ethernet card",0, NF },
                        { 0x0040, "PRO/100 S Desktop Adapter PCI 10/100Mb/s ethernet card",0, NF },
 	  /* Seattle AL440BX is 0x8080, is anything else ? */
                        { 0x8080, "motherboard",0, NF },
                        { 0x3013, "Integrated LAN (82562ET)",0, NF },
                        { 0x4541, "Eastern (D815EEA) motherboard",0, NF },
                        { 0x4d55, "Maui (MU) motherboard",0, NF },
#endif
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_MATROX, {
                        { 0x1100, "Mystique",0, NF },
                        { 0x1000, "Millennium II",0, NF },
                        { 0x0100, "Millennium II",0, NF },
                        { 0x1200, "Millennium II",0, NF },
                        { PCI_CARD_MILL_G200_SD, "Millennium G200 SD",0, NF },
                        { PCI_CARD_PROD_G100_SD, "Produktiva G100 SD",0, NF },
                        { PCI_CARD_MYST_G200_SD, "Mystique G200 SD",0, NF },
                        { PCI_CARD_MILL_G200_SG, "Millennium G200 SG",0, NF },
                        { PCI_CARD_MARV_G200_SD, "Marvel G200 SD",0, NF },
                        { 0x1001, "Productiva G100 SG",0, NF },
                        { PCI_CARD_G400_TH, "G400 Twin Head",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_SIS, {
                        { 0x6306, "530 based motherboard",0, NF },
                        { 0x6326, "6326 based card",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#ifdef VENDOR_INCLUDE_NONVIDEO
	{ PCI_VENDOR_CREATIVE, {
			{ 0x4c4c, "Sound Blaster PCI128",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#endif
	{ PCI_VENDOR_S3, {
                        { 0x8904, "Trio3D",0, NF },
                        { 0x8a10, "Generic",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_NUMNINE, {
                        { 0x8a10, "Reality 334",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_TOSHIBA, {
                        { 0x0001, "4010CDT CT65555",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#ifdef VENDOR_INCLUDE_NONVIDEO
	{ PCI_VENDOR_LITEON, {
			{ 0xc001, "LNE100TX Version 2.0",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_BUSLOGIC, {
	                { 0x1040,	"BT958",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
	{ PCI_VENDOR_NETGEAR, {
			{ 0xf004, "FA310-TX Rev. D2",0, NF },
                        { 0x0000, (char *)NULL,0, NF } } },
#endif
#if 0
	{ PCI_VENDOR_VMWARE, {
    			{PCI_CHIP_VMWARE0405,	"PCI SVGA (FIFO)",0, NF },
    			{PCI_CHIP_VMWARE0710,	"LEGACY SVGA",0, NF },
			{0x0000,		NULL,0, NF } } },
#endif
	{0x0000, {
	  		{0x0000,  NULL,0, NF } } },
};
#endif
#endif
#endif /* _XF86_PCIINFO_H */
