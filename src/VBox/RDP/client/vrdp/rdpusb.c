/** @file
 *
 * Remote Desktop Protocol client:
 * USB Channel Process Functions
 *
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "../rdesktop.h"

#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>

#include "vrdpusb.h"

#define RDPUSB_REQ_OPEN              (0)
#define RDPUSB_REQ_CLOSE             (1)
#define RDPUSB_REQ_RESET             (2)
#define RDPUSB_REQ_SET_CONFIG        (3)
#define RDPUSB_REQ_CLAIM_INTERFACE   (4)
#define RDPUSB_REQ_RELEASE_INTERFACE (5)
#define RDPUSB_REQ_INTERFACE_SETTING (6)
#define RDPUSB_REQ_QUEUE_URB         (7)
#define RDPUSB_REQ_REAP_URB          (8)
#define RDPUSB_REQ_CLEAR_HALTED_EP   (9)
#define RDPUSB_REQ_CANCEL_URB        (10)
#define RDPUSB_REQ_DEVICE_LIST       (11)
#define RDPUSB_REQ_NEGOTIATE         (12)

static VCHANNEL *rdpusb_channel;

#define PROC_BUS_USB "/proc/bus/usb"

/* A device list entry */
#pragma pack (1)
typedef struct _DevListEntry
{
	uint16_t oNext;                /* Offset of the next structure. 0 if last. */
	uint32_t id;                   /* Identifier of the device assigned by the client. */
	uint16_t bcdUSB;               /* USB verion number. */
	uint8_t bDeviceClass;          /* Device class. */
	uint8_t bDeviceSubClass;       /* Device subclass. */
	uint8_t bDeviceProtocol;       /* Device protocol. */
	uint16_t idVendor;             /* Vendor id. */
	uint16_t idProduct;            /* Product id. */
	uint16_t bcdRev;               /* Revision. */
	uint16_t oManufacturer;        /* Offset of manufacturer string. */
	uint16_t oProduct;             /* Offset of product string. */
	uint16_t oSerialNumber;        /* Offset of serial string. */
	uint16_t idPort;               /* Physical USB port the device is connected to. */
} DevListEntry;
#pragma pack ()

static uint16 getBcd (const char *str, const char *prefix)
{
	char *p = strstr (str, prefix);
	if (p)
	{
		uint16 h, l;

		p += strlen (prefix);

		while (*p == ' ') p++; // skiping spaces

		h = (uint16)strtoul (p, &p, 10) & 0xFF;

		if (*p == '.')
		{
			l = (uint16)strtoul (p + 1, NULL, 10) & 0xFF;

			return (h << 8) + l;
		}
	}

	Log(("Could not get BCD '%s' in line [%s]\n", prefix, str));

	return 0;
}

static uint8 getU8 (const char *str, const char *prefix)
{
	char *p = strstr (str, prefix);
	if (p)
	{
		p += strlen (prefix);
		while (*p == ' ') p++; // skiping spaces
		return ((uint8)strtoul(p, NULL, 10) & 0xFF);
	}

	Log(("Could not get U8 '%s' in line [%s]\n", prefix, str));

	return 0;
}

static uint16 getU16 (const char *str, const char *prefix)
{
	char *p = strstr (str, prefix);
	if (p)
	{
		uint16 tmp16;
		p += strlen (prefix);
		while (*p == ' ') p++; // skiping spaces
		tmp16 = (uint16)strtoul (p, NULL, 16);
		return (tmp16);
	}

	Log(("Could not get U16 '%s' in line [%s]\n", prefix, str));

	return 0;
}

static char *getString (const char *str, const char *prefix)
{
	char *p = strstr (str, prefix);
	if (p)
	{
		return (char *)p + strlen (prefix);
	}

	return 0;
}


static void *build_device_list (int *plen)
{
	Log(("RDPUSB build_device_list"));

	uint8 tmp8;

	uint32 size = 0;
	uint32 len = 0;
	uint32 lastlen = 0;
	uint8 *buffer = NULL;

	char szLine[1024];
	char *p;

	DevListEntry *e = NULL;

	FILE *f = fopen (PROC_BUS_USB "/devices", "r");

	if (f != NULL)
	{
		while (fgets (szLine, sizeof (szLine), f))
		{
			p = strchr (szLine, '\n');
			if (p) *p = 0;

			p = &szLine[0];

//			Log(("%s\n", szLine));

			switch (*p)
			{
				case 'T':
				{
					/* A new device, allocate memory. */
					if (size - len < sizeof (DevListEntry))
					{
						buffer = (uint8*)xrealloc (buffer, size + sizeof (DevListEntry));
						size += sizeof (DevListEntry);
					}

					if (len > 0)
					{
						e = (DevListEntry *)&buffer[lastlen];

						char path[128];

						sprintf (path, PROC_BUS_USB "/%03d/%03d", (e->id>>8) & 0xFF, e->id & 0xFF);

						Log(("%s: id = %X, class = %d access = %d\n", path, e->id, e->bDeviceClass, access (path, R_OK | W_OK)));

						if (e->id != 0 && e->bDeviceClass != 9 && access (path, R_OK | W_OK) == 0) // skip hubs as well
						{
							Log(("Added device vendor %04X, product %04X\n", e->idVendor, e->idProduct));
							e->oNext = len - lastlen;
						}
						else
						{
							/* The entry was not filled. Reuse.*/
							len = lastlen;
						}
					}

					lastlen = len;

					e = (DevListEntry *)&buffer[lastlen];
					memset (e, 0, sizeof (DevListEntry));

					len += sizeof (DevListEntry);

					// T:  Bus=03 Lev=02 Prnt=36 Port=01 Cnt=02 Dev#= 38 Spd=12  MxCh= 0
					/* Get bus and dev#. */
					tmp8 = getU8 (szLine, "Bus=");
					if (!tmp8)
					{
						e->id = 0;
					}
					else
					{
						e->idPort &= 0x00FF;
						e->idPort |= (uint16)tmp8 << 8;
						e->id &= 0x00FF;
						e->id |= (uint16)tmp8 << 8;
					}

					if (e)
					{
						tmp8 = getU8 (szLine, "Dev#=");
						if (!tmp8)
						{
							e->id = 0;
						}
						else
						{
							e->id &= 0xFF00;
							e->id |= tmp8;
						}

						if (e->id != 0)
						{
							e->idPort &= 0xFF00;
							e->idPort |= getU8 (szLine, "Port=");
						}
					}
				} break;
				case 'D':
				{
					// D:  Ver= 1.00 Cls=00(>ifc ) Sub=00 Prot=00 MxPS= 8 #Cfgs=  1
					if (e && e->id)
					{
					        e->bcdUSB = getBcd (szLine, "Ver=");
						e->bDeviceClass = getU8 (szLine, "Cls=");
						e->bDeviceSubClass = getU8 (szLine, "Sub=");
						e->bDeviceProtocol = getU8 (szLine, "Prot=");
					}
				} break;
				case 'P':
				{
					// P:  Vendor=0483 ProdID=2016 Rev= 0.01
					if (e && e->id)
					{
						e->idVendor = getU16 (szLine, "Vendor=");
						e->idProduct = getU16 (szLine, "ProdID=");
					        e->bcdRev = getBcd (szLine, "Rev=");
					}

				} break;
				case 'S':
				{
					if (e && e->id)
					{
						// S:  Manufacturer=STMicroelectronics
						uint16 offset = len - lastlen;
						uint16 offset_addr = 0;

						char *s = getString (szLine, "Manufacturer=");
						if (s)
						{
							offset_addr = (uint8 *)&e->oManufacturer - (uint8 *)e;
						}
						else
						{
							s = getString (szLine, "Product=");

							if (s)
							{
								offset_addr = (uint8 *)&e->oProduct - (uint8 *)e;
							}
							else
							{
								s = getString (szLine, "SerialNumber=");

								if (s)
								{
									offset_addr = (uint8 *)&e->oSerialNumber - (uint8 *)e;
								}
							}
						}

						if (s)
						{
							int l = strlen (s) + 1;

							if (l > 1)
							{
								if (size - len < l)
								{
									buffer = (uint8*)xrealloc (buffer, size + l);
									size += l;
									e = (DevListEntry *)&buffer[lastlen];
								}

								memcpy (&buffer[len], s, l);
								len += l;

								*(uint16 *)((uint8 *)e + offset_addr) = offset;
							}
							else
							{
								*(uint16 *)((uint8 *)e + offset_addr) = 0;
							}
						}
					}
				} break;
			}
		}

		if (len > 0)
		{
			Log(("Finalising list\n"));
#ifdef RDPUSB_DEBUG
			hexdump(buffer, lastlen);
#endif

			e = (DevListEntry *)&buffer[lastlen];

			char path[128];

			sprintf (path, PROC_BUS_USB "/%03d/%03d", (e->id>>8) & 0xFF, e->id & 0xFF);

			if (e->id != 0 && e->bDeviceClass != 9 && access (path, R_OK | W_OK) == 0) // skip hubs as well
			{
				Log(("Added device vendor %04X, product %04X\n", e->idVendor, e->idProduct));
				e->oNext = len - lastlen;
			}
			else
			{
				/* The entry was not filled. Reuse.*/
				len = lastlen;
			}

			lastlen = len;

			if (size - len < 2)
			{
				buffer = (uint8*)xrealloc (buffer, size + 2);
				size += 2;
			}

			e = (DevListEntry *)&buffer[lastlen];
			e->oNext = 0;
			lastlen += 2;
		}

	        fclose (f);
	}

	*plen = lastlen;
	return buffer;
}

static STREAM
rdpusb_init_packet(uint32 len, uint8 code)
{
	STREAM s;

	s = channel_init(rdpusb_channel, len + 5);
	out_uint32_le (s, len + sizeof (code)); /* The length of data after the 'len' field. */
	out_uint8(s, code);
	return s;
}

static void
rdpusb_send(STREAM s)
{
#ifdef RDPUSB_DEBUG
	Log(("RDPUSB send:\n"));
	hexdump(s->channel_hdr + 8, s->end - s->channel_hdr - 8);
#endif

	channel_send(s, rdpusb_channel);
}

static void
rdpusb_send_reply (uint8_t code, uint8_t status, uint32_t devid)
{
	STREAM s = rdpusb_init_packet(1, code);
	out_uint8(s, status);
	out_uint32_le(s, devid);
	s_mark_end(s);
	rdpusb_send(s);
}

static void
rdpusb_send_access_denied (uint8_t code, uint32_t devid)
{
    rdpusb_send_reply (code, VRDP_USB_STATUS_ACCESS_DENIED, devid);
}

static inline int
vrdp_usb_status (int rc, VUSBDEV *pdev)
{
	if (!rc || pdev->request_detach)
	{
		return VRDP_USB_STATUS_DEVICE_REMOVED;
	}

	return VRDP_USB_STATUS_SUCCESS;
}

static struct usb_proxy *g_proxies = NULL;

static struct usb_proxy *
devid2proxy (uint32_t devid)
{
	struct usb_proxy *proxy = g_proxies;

	while (proxy && proxy->devid != devid)
	{
		proxy = proxy->next;
	}

	return proxy;
}

static void
rdpusb_reap_urbs (void)
{
	STREAM s;

	PVUSBURB pUrb = NULL;

	struct usb_proxy *proxy = g_proxies;

	while (proxy)
	{
		pUrb = op_usbproxy_back_reap_urb(proxy, 0);

		if (pUrb)
		{
			int datalen = 0;

			Log(("RDPUSB: rdpusb_reap_urbs: cbData = %d, enmStatus = %d\n", pUrb->cbData, pUrb->enmStatus));

			if (pUrb->enmDir == VUSB_DIRECTION_IN)
			{
				datalen = pUrb->cbData;
			}

			s = rdpusb_init_packet(14 + datalen, RDPUSB_REQ_REAP_URB);
			out_uint32_le(s, proxy->devid);
			out_uint8(s, VRDP_USB_REAP_FLAG_LAST);
			out_uint8(s, pUrb->enmStatus);
			out_uint32_le(s, pUrb->handle);
			out_uint32_le(s, pUrb->cbData);

			if (datalen)
			{
				out_uint8a (s, pUrb->abData, datalen);
			}

			s_mark_end(s);
			rdpusb_send(s);

			if (pUrb->prev || pUrb->next || pUrb == proxy->urbs)
			{
				/* Remove the URB from list. */
				if (pUrb->prev)
				{
					pUrb->prev->next = pUrb->next;
				}
				else
				{
					proxy->urbs = pUrb->next;
				}

				if (pUrb->next)
				{
					pUrb->next->prev = pUrb->prev;
				}
			}

#ifdef RDPUSB_DEBUG
			Log(("Going to free %p\n", pUrb));
#endif
			xfree (pUrb);
#ifdef RDPUSB_DEBUG
			Log(("freed %p\n", pUrb));
#endif
		}

		proxy = proxy->next;
	}

	return;
}

static void
rdpusb_process(STREAM s)
{
	int rc;

	uint32 len;
	uint8 code;
	uint32 devid;

	struct usb_proxy *proxy = NULL;

#ifdef RDPUSB_DEBUG
	Log(("RDPUSB recv:\n"));
	hexdump(s->p, s->end - s->p);
#endif

	in_uint32_le (s, len);
	if (len > s->end - s->p)
	{
		error("RDPUSB: not enough data len = %d, bytes left %d\n", len, s->end - s->p);
		return;
	}

	in_uint8(s, code);

	Log(("RDPUSB recv: len = %d, code = %d\n", len, code));

	switch (code)
	{
		case RDPUSB_REQ_OPEN:
		{
			char devpath[128];

	        	in_uint32_le(s, devid);

			proxy = (struct usb_proxy *)xmalloc (sizeof (struct usb_proxy));

			memset (proxy, 0, sizeof (struct usb_proxy));

			proxy->Dev.pszName = "Remote device";
			proxy->devid = devid;

			sprintf (devpath, PROC_BUS_USB "/%03d/%03d", (devid>>8) & 0xFF, devid & 0xFF);

			rc = op_usbproxy_back_open(proxy, devpath);

			if (rc != VINF_SUCCESS)
			{
			        rdpusb_send_access_denied (code, devid);
				xfree (proxy);
				proxy = NULL;
			}
			else
			{
				if (g_proxies)
				{
					g_proxies->prev = proxy;
				}

				proxy->next = g_proxies;
				g_proxies = proxy;
			}
		} break;

		case RDPUSB_REQ_CLOSE:
		{
	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (proxy)
			{
				op_usbproxy_back_close(proxy);

				if (proxy->prev)
				{
					proxy->prev->next = proxy->next;
				}
				else
				{
					g_proxies = proxy->next;
				}

				if (proxy->next)
				{
					proxy->next->prev = proxy->prev;
				}

				xfree (proxy);
				proxy = NULL;
			}

			/* No reply. */
		} break;

		case RDPUSB_REQ_RESET:
		{
	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

			rc = op_usbproxy_back_reset(proxy);

			if (rc != VINF_SUCCESS)
			{
				rdpusb_send_reply (code, vrdp_usb_status (!rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_SET_CONFIG:
		{
			uint8 cfg;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint8(s, cfg);

			rc = op_usbproxy_back_set_config(proxy, cfg);

			if (!rc)
			{
				rdpusb_send_reply (code, vrdp_usb_status (rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_CLAIM_INTERFACE:
		{
			uint8 ifnum;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint8(s, ifnum);

			rc = op_usbproxy_back_claim_interface(proxy, ifnum);

			if (!rc)
			{
				rdpusb_send_reply (code, vrdp_usb_status (rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_RELEASE_INTERFACE:
		{
			uint8 ifnum;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint8(s, ifnum);

			rc = op_usbproxy_back_release_interface(proxy, ifnum);

			if (!rc)
			{
				rdpusb_send_reply (code, vrdp_usb_status (rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_INTERFACE_SETTING:
		{
			uint8 ifnum;
			uint8 setting;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint8(s, ifnum);
	        	in_uint8(s, setting);

			rc = op_usbproxy_back_interface_setting(proxy, ifnum, setting);

			if (!rc)
			{
				rdpusb_send_reply (code, vrdp_usb_status (rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_QUEUE_URB:
		{
			uint32 handle;
			uint8 type;
			uint8 ep;
			uint8 dir;
			uint32 urblen;
			uint32 datalen;

			PVUSBURB pUrb; // struct vusb_urb *urb;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				/* No reply. */
				break;
			}

	        	in_uint32(s, handle);
	        	in_uint8(s, type);
	        	in_uint8(s, ep);
	        	in_uint8(s, dir);
	        	in_uint32(s, urblen);
	        	in_uint32(s, datalen);

			/* Allocate a single block for URB description and data buffer */
			pUrb = (PVUSBURB)xmalloc (sizeof (VUSBURB) +
			                          (urblen <= sizeof (pUrb->abData)? 0: urblen - sizeof (pUrb->abData))
						 );
			memset (pUrb, 0, sizeof (VUSBURB));
			pUrb->pDev = &proxy->Dev;
			pUrb->handle = handle;
			pUrb->enmType = type;
			pUrb->enmStatus = 0;
			pUrb->EndPt = ep;
			pUrb->enmDir = dir;
			pUrb->cbData = urblen;

			Log(("RDPUSB: queued URB handle = %d\n", handle));

			if (datalen)
			{
				in_uint8a (s, pUrb->abData, datalen);
			}

			rc = op_usbproxy_back_queue_urb(pUrb);

			/* No reply required. */

			if (rc)
			{
				if (proxy->urbs)
				{
					proxy->urbs->prev = pUrb;
				}

				pUrb->next = proxy->urbs;
				proxy->urbs = pUrb;
			}
			else
			{
				xfree (pUrb);
			}
		} break;

		case RDPUSB_REQ_REAP_URB:
		{
			rdpusb_reap_urbs ();
		} break;

		case RDPUSB_REQ_CLEAR_HALTED_EP:
		{
			uint8 ep;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint8(s, ep);

			rc = op_usbproxy_back_clear_halted_ep(proxy, ep);

			if (!rc)
			{
				rdpusb_send_reply (code, vrdp_usb_status (rc, &proxy->Dev), devid);
			}
		} break;

		case RDPUSB_REQ_CANCEL_URB:
		{
			uint32 handle;
			PVUSBURB pUrb = NULL;

	        	in_uint32_le(s, devid);
			proxy = devid2proxy (devid);

			if (!proxy)
			{
				rdpusb_send_access_denied (code, devid);
				break;
			}

	        	in_uint32_le(s, handle);

			pUrb = proxy->urbs;

			while (pUrb && pUrb->handle != handle)
			{
				pUrb = pUrb->next;
			}

			if (pUrb)
			{
				op_usbproxy_back_cancel_urb(pUrb);

				/* No reply required. */

				/* Remove URB from list. */
				if (pUrb->prev)
				{
					pUrb->prev->next = pUrb->next;
				}
				else
				{
					proxy->urbs = pUrb->next;
				}

				if (pUrb->next)
				{
					pUrb->next->prev = pUrb->prev;
				}

				pUrb->next = pUrb->prev = NULL;

				Log(("Cancelled URB %p\n", pUrb));

				// xfree (pUrb);
			}
		} break;

		case RDPUSB_REQ_DEVICE_LIST:
		{
			void *buf = NULL;
			int len = 0;

			buf = build_device_list (&len);

			s = rdpusb_init_packet(len? len: 2, code);
			if (len)
			{
				out_uint8p (s, buf, len);
			}
			else
			{
				out_uint16_le(s, 0);
			}
			s_mark_end(s);
			rdpusb_send(s);

			if (buf)
			{
				free (buf);
			}
		} break;

		case RDPUSB_REQ_NEGOTIATE:
		{
			s = rdpusb_init_packet(1, code);
			out_uint8(s, VRDP_USB_CAPS_FLAG_ASYNC);
			s_mark_end(s);
			rdpusb_send(s);
		} break;

		default:
			unimpl("RDPUSB code %d\n", code);
			break;
	}
}

void
rdpusb_add_fds(int *n, fd_set * rfds, fd_set * wfds)
{
	struct usb_proxy *proxy = g_proxies;

//	Log(("RDPUSB: rdpusb_add_fds: begin *n = %d\n", *n));

	while (proxy)
	{
		int fd = dev2fd(proxy);

		if (fd != -1)
		{
//		        Log(("RDPUSB: rdpusb_add_fds: adding %d\n", proxy->priv.File));

			FD_SET(fd, rfds);
			FD_SET(fd, wfds);
			*n = MAX(*n, fd);
		}

		proxy = proxy->next;
	}

//	Log(("RDPUSB: rdpusb_add_fds: end *n = %d\n", *n));

	return;
}

void
rdpusb_check_fds(fd_set * rfds, fd_set * wfds)
{
	(void)rfds;
	(void)wfds;

//	Log(("RDPUSB: rdpusb_check_fds: begin\n"));

	rdpusb_reap_urbs ();

//	Log(("RDPUSB: rdpusb_check_fds: end\n"));

	return;
}

RD_BOOL
rdpusb_init(void)
{
	rdpusb_channel =
		channel_register("vrdpusb", CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP,
				 rdpusb_process);
	return (rdpusb_channel != NULL);
}

void
rdpusb_close (void)
{
	struct usb_proxy *proxy = g_proxies;

	while (proxy)
	{
		struct usb_proxy *next = proxy->next;

		Log(("RDPUSB: closing proxy %p\n", proxy));

		op_usbproxy_back_close(proxy);
		xfree (proxy);

		proxy = next;
	}

	return;
}
