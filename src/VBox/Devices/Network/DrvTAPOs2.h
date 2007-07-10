/* 
 * (C) 2007 Willibald Meyer <mywi-src-spam@isdnpm.de>
 */

#ifndef ___DrvTAPOS2_H___
#define ___DrvTAPOS2_H___

#define PROT_CATEGORY              0x97

#define SNIFFER_READ_PACKET        0x10
#define SNIFFER_READ_MODULE_NAME   0x11
#define SNIFFER_READ_LAN_NAME      0x12
#define SNIFFER_READ_LAN_ID        0x13
#define SNIFFER_SET_QUEUE_MODE     0x14
#define SNIFFER_RESET_QUEUE_MODE   0x15

#define TAP_READ_MAC_ADDRESS       0x20
#define TAP_READ_PACKET            0x21
#define TAP_WRITE_PACKET           0x22
#define TAP_GET_LAN_NUMBER         0x23
#define TAP_CANCEL_READ            0x24
#define TAP_ENABLE_READ            0x25
#define TAP_DISABLE_READ           0x26
/* The next 4 aren't handled by StratIoctlTap... */
#define TAP_ADD_SWITCH             0x27
#define TAP_DEL_SWITCH             0x28

#define TAP_ENABLE_PROXY_ARP       0x29
#define TAP_DISABLE_PROXY_ARP      0x2A

#define TAP_CONNECT_NIC            0x70
#define TAP_DISCONNECT_NIC         0x71

#endif

