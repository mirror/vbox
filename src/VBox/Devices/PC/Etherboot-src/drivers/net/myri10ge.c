/************************************************* -*- linux-c -*-
 * Myricom 10Gb Network Interface Card Software
 * Copyright 2006, Myricom, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************/
 
/* to get some global routines like printf */
#include "etherboot.h"
#include "lib.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
#include "timer.h"

#include "myri/lanai_Z8E_def.h"
#include "myri/mcp_gen_header.h"
#include "myri/myri10ge_mcp.h"
 
/****************************************************************
 * Overrides
 ****************************************************************/
 
/* Etherboot will never require more than 1 segment per packet. */
 
#undef MYRI10GE_MCP_ETHER_MAX_SEND_DESC
#define MYRI10GE_MCP_ETHER_MAX_SEND_DESC 1
 
/****************************************************************
 * Utilities
 ****************************************************************/
 
#define NUM_ELEM(x) (sizeof((x))/sizeof((x)[1]))
#define assert(x) do {							\
	if (x) break;							\
	printf("assertion failed in file %s, function %s(), line %d\n",	\
	       __FILE__, __FUNCTION__, __LINE__);			\
	while (1);							\
} while (0)
 
/****************************************************************
 * Linux emulation
 *
 * Implement just enough of the Linux kernel network API that we
 * can use the myrige/linux/myri10ge.c code (the Myricom Linux driver).
 ****************************************************************/
 
#define NOP do{}while(0)
#define SUCCEED 0		/* emulate function returning 0 */
#define IGNORE_SEMICOLON extern char ignore_semicolon[]
 
/****************
 * Misc. fakeouts.
 ****************/

#define pci_dev pci_device	/* redirect struct refs in Linux code. */
static void *drvdata;
#define ETH_DATA_LEN 1500
#define pci_set_drvdata(a,b) (drvdata=(b))
static void *pci_get_drvdata (struct pci_dev *pdev __unused) {return drvdata;}
#define module_param(a,b,c) IGNORE_SEMICOLON
#define charp char *
#define kfree(a) NOP
#define likely(a) (a)
#define unlikely(a) (a)
#define dev_err(dev,msg) printf ("%s\n", msg)
#define printk printf
#define KERN_ERR "ERROR: "
enum { EAGAIN=1, ENXIO, EBUSY, ENOBUFS, ENOMEM, ENODEV, EOPNOTSUPP };
typedef uint32_t dma_addr_t;
#define jiffies 0		/* used only to record ignored times */
#define dev_info(a,b) NOP
#define alloc_etherdev(x) (preallocated_net_device.priv = &_mgp,	\
			   &preallocated_net_device)

/****************
 * Work queues (disabled)
 ****************/

struct work_struct { unsigned int unused; };
#define schedule_work(a) NOP
#define INIT_WORK(a,b,c) NOP
#define flush_scheduled_work() NOP

/****************
 * Network device
 ****************/

#define free_netdev(n) NOP	/* we use a preallocated netdev */
#define unregister_netdev(n) NOP

static struct net_device {
	int last_rx;		/* set, but never used */
	int trans_start;	/* set, but never used */
	const char *name;
	uint16_t mtu;
	uint8_t dev_addr[6];
	struct myri10ge_priv *priv;
	enum {
		NETDEV_TX_DISABLED=1, NETDEV_CARRIER_OFF=2, NETDEV_TX_STOPPED=4
	} state;
} preallocated_net_device;
#define netdev_priv(netdev) ((netdev)->priv)

struct net_device_stats 
{
	unsigned int rx_bytes;
	unsigned int rx_dropped;
	unsigned int rx_packets;
	unsigned int tx_bytes;
	unsigned int tx_dropped;
	unsigned int tx_packets;
};
 
/****************
 * spinlocks -- ignore the calls
 ****************/
 
typedef int spinlock_t;
#define spin_lock_init(spin_lock_ptr) (*(spin_lock_ptr) = 0)
#define spin_lock(spin_lock_ptr) NOP
#define spin_unlock(spin_lock_ptr) NOP
 
/****************
 * PCI -- etherboot uses a trivial virt_to_bus() interface
 ****************/

struct pci_device_id;
#define DECLARE_PCI_UNMAP_ADDR(bus) /* We don't record, so don't declare. */
#define DECLARE_PCI_UNMAP_LEN(len) /* We don't record, so don't declare. */
#define pci_map_page(dev,pg,pgoff,len,dir) 0 /* unused, since no frags */
static dma_addr_t pci_map_single (struct pci_dev *dev __unused,
				  void *ptr,
				  unsigned long len __unused,
				  int dir __unused)
{
	return virt_to_bus (ptr);
}
#define pci_name(dev) "unknown"
#define pci_enable_device(a) SUCCEED
#define pci_free_consistent(a,b,ptr,c) NOP
#define pci_unmap_addr(a,bus) 0	/* not recorded */
#define pci_unmap_addr_set(a,field,addr) NOP /* don't care so don't record. */
#define pci_unmap_len(a,len) 0	/* not recorded */
#define pci_unmap_len_set(a,field,len) NOP /* don't care, so don't record. */
#define pci_unmap_page(pdev,bus,unmap_len,dir) NOP
static void pci_unmap_single (struct pci_dev *dev __unused,
			      unsigned int bus __unused,
			      unsigned long unmap_len __unused,
			      int dir __unused)
{
	return;
}
enum {PCI_DMA_FROMDEVICE, PCI_DMA_TODEVICE};
 
/****************
 * sk_buffs
 ****************/
 
/* Some fairly simple sk_buff support for our always-linear buffers. */
 
enum {CHECKSUM_NONE, CHECKSUM_HW};
struct sk_buff 
{
	struct sk_buff *next;	/* MUST BE FIRST field in struct */
	struct sk_buff **free_list; /* where to free the skb */
	void *allocation;	/* start of buffer */
	unsigned char *data;	/* start of packet */
	unsigned char *tail;	/* send of packet */
	unsigned int len;	/* packet length */
	/****
	 * The rest of this struct exists only to placate the Linux code.
	 ****/
	struct net_device *dev;	/* set, but otherwise ignored */
	unsigned short csum;	/* set, but otherwise ignored */
	unsigned char protocol;	/* set, but otherwise ignored */
	unsigned char ip_summed; /* mostly ignored */
	union { unsigned char *raw; } h; /* only in dead code */
	unsigned int data_len;	/* packet data in frags (always zero) */
	unsigned int truesize;	/* adjusted, but otherwise ignored */
	struct
	{
		unsigned int nr_frags; /* always zero, so frags below will
					  not be accessed. */
		unsigned int tso_size; /* always zero: no TSO */
		struct skb_frag_struct { /* only in dead code */
			unsigned int size; /* only in dead code */
			unsigned int page; /* only in dead code */
			unsigned int page_offset; /* only in dead code */
		}frags[0];	/* only in dead code */
	} shinfo;
};
static struct sk_buff *free_small_rx_skb;
static struct sk_buff *free_big_rx_skb;
static struct sk_buff *free_tx_skb;
/* This is called only for receive skbs. */
static struct sk_buff *dev_alloc_skb (unsigned int len)
{
	struct sk_buff **free_list;
	struct sk_buff *skb;
 
	/* FIXME: magic number */
	free_list = (len <= 128) ? &free_small_rx_skb : &free_big_rx_skb;
	skb = *free_list;
	if (unlikely (!skb))
		return 0; 
	*free_list = skb->next;
	return skb;
}
#define dev_kfree_skb_irq(skb) dev_kfree_skb_any (skb) /* FIXME: unsafe */
static void dev_kfree_skb_any (struct sk_buff *skb)
{
	struct sk_buff **free_list;
	void *buf;
 
	/* Reinitialize the sk_buff. */
	
	free_list = skb->free_list;
	buf = skb->allocation;
	memset (skb, 0, sizeof (*skb));
	skb->tail = skb->data = skb->allocation = buf;
	skb->free_list = free_list;
	if (CHECKSUM_NONE != 0)
		skb->ip_summed = CHECKSUM_NONE;
 
	/* Free it. */
	
	skb->next = *free_list;
	*free_list = skb;
}
static void create_skb (char *buf, struct sk_buff **fl, unsigned int len)
{
	struct sk_buff *skb;
	
	/* Roundup the length, so we can safely put the skb after it. */
	skb = (struct sk_buff *)(buf + len + (-len & 7));
	skb->free_list = fl;
	skb->allocation = buf;
	dev_kfree_skb_any (skb);
}
/* Skip bytes at the start, realigning the buffer before use. */
static inline void skb_reserve(struct sk_buff *s, unsigned int len)
{
	s->data += len;
	s->tail += len;
}
#define skb_linearize(skb,mode) SUCCEED /* All buffs are linear */
/* Zero the bytes after a tiny packet, in case the NIC DMAs them. */
static inline struct sk_buff *skb_padto(struct sk_buff *skb, unsigned int len)
{
	unsigned int size = skb->len;
	if (likely(size >= len))
		return skb;
	memset (skb->data+size, 0, len-size);
	return skb;
}
/* Allocate buffering for len bytes. */
static inline unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb->tail;
	skb->tail += len;
	skb->len  += len;
	return tmp;
}
#define skb_shinfo(skb) (&(skb)->shinfo)
#define eth_type_trans(skb,dev) 0 /* dummy ignored value */
 
/****************
 * Network interface
 ****************/

static struct sk_buff *rx_first;
static struct sk_buff *rx_last = (struct sk_buff *) &rx_first;
/* append the skb to the receive FIFO. */
#define netif_tx_disable(dev) do{(dev)->state|=NETDEV_TX_DISABLED;}while(0)
#define netif_rx(skb) ((void)((rx_last = rx_last->next = (skb))->next = 0))
#define netif_stop_queue(dev) do{(dev)->state|=NETDEV_TX_STOPPED;}while(0)
#define netif_queue_stopped(dev) ((dev)->state & NETDEV_TX_STOPPED)
#define netif_wake_queue(dev) do{(dev)->state&=~NETDEV_TX_STOPPED;}while(0)
#define netif_carrier_on(dev) do{(dev)->state&=~NETDEV_CARRIER_OFF;}while(0)
#define netif_carrier_off(dev) do{(dev)->state|=NETDEV_CARRIER_OFF;}while(0)
 
/****************
 * Interrupts
 ****************/
 
struct pt_regs;
typedef enum {IRQ_NONE, IRQ_CLAIM, IRQ_HANDLED} irqreturn_t;
#define disable_irq(irq) NOP	/* since never enabled */
#define free_irq(irq,ctx) NOP	/* since never used */
#define request_irq(a,b,c,d,e) SUCCEED
 
/****************************************************************
 * Globals
 ****************************************************************/

#define MYRI10GE_MAX_ETHER_MTU 1514

#define MYRI10GE_ETH_STOPPED 0
#define MYRI10GE_ETH_STOPPING 1
#define MYRI10GE_ETH_STARTING 2
#define MYRI10GE_ETH_RUNNING 3
#define MYRI10GE_ETH_OPEN_FAILED 4

#define MYRI10GE_EEPROM_STRINGS_SIZE 256
#define MYRI10GE_MCP_ETHER_MAX_SEND_DESC_TSO ((65536 / 2048) * 2)

#ifndef PCI_CAP_ID_VNDR
#define  PCI_CAP_ID_VNDR 0x9
#endif

#ifndef PCI_CAP_ID_EXP
#define  PCI_CAP_ID_EXP 0x10
#endif

#ifndef PCI_EXP_DEVCTL
#define PCI_EXP_DEVCTL 0x8
#endif

#ifndef PCI_EXP_DEVCTL_READRQ
#define PCI_EXP_DEVCTL_READRQ 0x7000
#endif

struct myri10ge_rx_buffer_state {
	struct sk_buff *skb;
        DECLARE_PCI_UNMAP_ADDR(bus)
        DECLARE_PCI_UNMAP_LEN(len)
};

struct myri10ge_tx_buffer_state {
	struct sk_buff *skb;
	int last;
        DECLARE_PCI_UNMAP_ADDR(bus)
        DECLARE_PCI_UNMAP_LEN(len)
};

typedef struct
{
  uint32_t data0;
  uint32_t data1;
  uint32_t data2;
} myri10ge_cmd_t;

typedef struct
{
	mcp_kreq_ether_recv_t *lanai;	/* lanai ptr for recv ring */
	volatile uint8_t *wc_fifo;	/* w/c rx dma addr fifo address */
	mcp_kreq_ether_recv_t *shadow;	/* host shadow of recv ring */
	struct myri10ge_rx_buffer_state *info;
	int cnt;
	int alloc_fail;
	int mask;			/* number of rx slots -1 */
	int lanai_mask;
} myri10ge_rx_buf_t;

typedef struct
{
	mcp_kreq_ether_send_t *lanai;	/* lanai ptr for sendq	*/
	volatile uint8_t *wc_fifo;	/* w/c send fifo address */
	mcp_kreq_ether_send_t *req_list;/* host shadow of sendq */
	char *req_bytes;
	struct myri10ge_tx_buffer_state *info;
	int req;			/* transmit slots submitted	*/
	int mask;			/* number of transmit slots -1	*/
	int lanai_mask;
	int done;			/* transmit slots completed	*/
	int pkt_done;			/* packets completed */
	int pkt_start;			/* packets started */
	int boundary;			/* boundary transmits cannot cross*/
} myri10ge_tx_buf_t;

typedef struct {
	mcp_slot_t *entry;
	dma_addr_t bus;
	int cnt;
	int idx;
} myri10ge_rx_done_t;

struct myri10ge_priv{
	int running;			/* running? 		*/
	int csum_flag;			/* rx_csums? 		*/
	myri10ge_tx_buf_t tx;	/* transmit ring 	*/
	myri10ge_rx_buf_t rx_small;
	myri10ge_rx_buf_t rx_big;
	myri10ge_rx_done_t rx_done;
	int small_bytes;
	struct net_device *dev;
	struct net_device_stats stats;
	volatile uint8_t *sram;
	int sram_size;
	unsigned long  board_span;
	unsigned long iomem_base;
	volatile uint32_t *irq_claim;
	volatile uint32_t *irq_deassert;
	char *mac_addr_string;
	mcp_cmd_response_t *cmd;
	dma_addr_t cmd_bus;
	mcp_irq_data_t *fw_stats;
	dma_addr_t fw_stats_bus;
	struct pci_dev *pdev;
	int msi_enabled;
	unsigned int link_state;
	unsigned int rdma_tags_available;
	int intr_coal_delay;
	volatile uint32_t *intr_coal_delay_ptr;
	int mtrr;
	spinlock_t cmd_lock;
	int wake_queue;
	int stop_queue;
	int down_cnt;
	struct work_struct watchdog_work;
	int watchdog_resets;
	int tx_linearized;
	int pause;
	char *fw_name;
	char eeprom_strings[MYRI10GE_EEPROM_STRINGS_SIZE];
	char fw_version[128];
	uint8_t	mac_addr[6];		/* eeprom mac address */
	int vendor_specific_offset;
	/****
	 * Etherboot additions.
	 ****/
	struct mcp_gen_header *mcp_hdr; /* struct is in network byte order */
};
struct myri10ge_priv _mgp;
 
#define TX_FILL_CNT 2		/* power of 2 */
#define RX_BIG_FILL_CNT 2	/* power of 2 */
#define RX_SMALL_FILL_CNT 8	/* power of 2 */
 
/* A structure that will be aligned on a 4kB bus address in the
   __shared memory region, to ensure alignment of the internal
   structures. */
 
struct preallocated
{
	/* power-of-two DMA buffers come first, in order of required
	   alignment size. */
	
	mcp_slot_t rx_done_entry[128]; /* 2048 */
	char tx_skb[TX_FILL_CNT][2048]; /* 2048 */
	char big_rx_skb[RX_BIG_FILL_CNT + 2][2048]; /* 2048 */
	char small_rx_skb[RX_SMALL_FILL_CNT + 2][256]; /* 256 */
	char fw_stats[1][64];	/* 64 */
	mcp_cmd_response_t cmd[1]; /* 8 */
	
	/* The rest are non-DMA buffers, requiring no special alignment. */
	
	char tx_req_bytes[8 + ((MYRI10GE_MCP_ETHER_MAX_SEND_DESC_TSO + 4)
			       * sizeof (mcp_kreq_ether_send_t))];
	mcp_kreq_ether_recv_t rx_big_shadow[RX_BIG_FILL_CNT];
	mcp_kreq_ether_recv_t rx_small_shadow[RX_SMALL_FILL_CNT];
	mcp_kreq_ether_send_t tx_req_list[TX_FILL_CNT];
	struct myri10ge_rx_buffer_state rx_big_info[RX_BIG_FILL_CNT];
	struct myri10ge_rx_buffer_state rx_small_info[RX_SMALL_FILL_CNT];
	struct myri10ge_tx_buffer_state tx_info[TX_FILL_CNT];
}; 
 
static char __preallocated[sizeof(struct preallocated) + 4095];
 
static struct preallocated *preallocated;
 
static void
preallocate (void) 
{
	unsigned int i;
 
	/* Bus-align and clear all buffers. */
	 
	preallocated = ((struct preallocated *)
			(__preallocated
			 + ((-virt_to_bus(__preallocated)) & 4095)));
	memset (preallocated, 0, sizeof (*preallocated));
 
	/* Clear all globals. */
 
	memset (&_mgp, 0, sizeof (_mgp));
 
	/* Initialize the skb free lists. */
 
	for (i=0; i<NUM_ELEM(preallocated->small_rx_skb); i++)
		create_skb (preallocated->small_rx_skb[i], &free_small_rx_skb,
			    128);
 
	for (i=0; i<NUM_ELEM(preallocated->big_rx_skb); i++)
		create_skb (preallocated->big_rx_skb[i], &free_big_rx_skb,
			    MYRI10GE_MAX_ETHER_MTU);
 
	for (i=0; i<NUM_ELEM(preallocated->tx_skb); i++)
		create_skb (preallocated->tx_skb[i], &free_tx_skb,
			    MYRI10GE_MAX_ETHER_MTU);
}

static char *myri10ge_fw_name = NULL;
static char *myri10ge_fw_unaligned = "ethp_z8e.dat";
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
static char *myri10ge_fw_aligned = "eth_z8e.dat";
static int myri10ge_ecrc_enable = 1;
#endif
static int myri10ge_max_intr_slots = 128;
static int myri10ge_small_bytes = -1; /* -1 == auto */
#ifdef CONFIG_PCI_MSI
static int myri10ge_msi = -1; /* 0: off, 1:on, otherwise auto */
#endif
static int myri10ge_intr_coal_delay = 25;
static int myri10ge_flow_control = 1;
static int myri10ge_skip_pio_read = 0;
static int myri10ge_force_firmware = 0;
static int myri10ge_skb_cross_4k = 0;
static int myri10ge_initial_mtu = MYRI10GE_MAX_ETHER_MTU - ETH_HLEN;

module_param(myri10ge_fw_name, charp, S_IRUGO | S_IWUSR);
module_param(myri10ge_max_intr_slots, int, S_IRUGO);
module_param(myri10ge_small_bytes, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_PCI_MSI
module_param(myri10ge_msi, int, S_IRUGO);
#endif
module_param(myri10ge_intr_coal_delay, int, S_IRUGO);
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
module_param(myri10ge_ecrc_enable, int, S_IRUGO);
#endif
module_param(myri10ge_flow_control, int, S_IRUGO);
module_param(myri10ge_skip_pio_read, int, S_IRUGO | S_IWUSR);
module_param(myri10ge_force_firmware, int, S_IRUGO);
module_param(myri10ge_skb_cross_4k, int, S_IRUGO | S_IWUSR);
module_param(myri10ge_initial_mtu, int, S_IRUGO);

/****************************************************************
 * Utility functions
 ****************************************************************/

/****************
 * MAC addr parsing
 ****************/

/* Convert a hex character to an interger */
#define XTOI(c) ('A' <= (c) && (c) <= 'F' ? (c) - 'A' + 10 :		\
		 'a' <= (c) && (c) <= 'f' ? (c) - 'a' + 10 :		\
		 (c) - '0')
/* Convert a pair of hex characters to an integer */
#define XXTOI(s) ((XTOI((s)[0]) << 4) | XTOI ((s)[1]))

static
int
string_specs_to_mac_addr (const char *string_specs, uint8_t mac_addr[6]) 
{
  int i;
  const char *s;

  memset (mac_addr, 0, 6);
  for (s=string_specs; *s; s+=strlen(s)+1) {
    if (!memcmp (s, "MAC=", 4)) {
      for (i=0; i<6; i++)
	mac_addr[i] = XXTOI (s+4+3*i);
      return 0;
    }
  }
  return 1;
}

/****************
 * PIO
 ****************/

#define MYRI10GE_FW_OFFSET 1024*1024
#define MYRI10GE_HIGHPART_TO_U32(X) \
(sizeof (X) == 8) ? ((uint32_t)((uint64_t)(X) >> 32)) : (0)
#define MYRI10GE_LOWPART_TO_U32(X) ((uint32_t)(X))

#define INVALID_DMA_ADDR 0


int myri10ge_hyper_msi_cap_on(struct pci_dev *pdev)
{
	uint8_t cap_off;
	int nbcap = 0;

	cap_off = PCI_CAPABILITY_LIST - 1;
	/* go through all caps looking for a hypertransport msi mapping */
	while (pci_read_config_byte(pdev, cap_off + 1, &cap_off) == 0 &&
	       nbcap++ <= 256/4) {
		uint32_t cap_hdr;
		if (cap_off == 0 || cap_off == 0xff)
			break;
		cap_off &= 0xfc;
		/* cf hypertransport spec, msi mapping section */
		if (pci_read_config_dword(pdev, cap_off, &cap_hdr) == 0
		    && (cap_hdr & 0xff) == 8 /* hypertransport cap */
		    && (cap_hdr & 0xf8000000) == 0xa8000000 /* msi mapping */
		    && (cap_hdr & 0x10000) /* msi mapping cap enabled */) {
			/* MSI present and enabled */
			return 1;
		}
	}
	return 0;
}

#ifdef CONFIG_PCI_MSI
static int
myri10ge_use_msi(struct pci_dev *pdev)
{
	if (myri10ge_msi == 1 || myri10ge_msi == 0)
		return myri10ge_msi;

	/*  find root complex for our device */
	while (pdev->bus && pdev->bus->self) {
		pdev = pdev->bus->self;
	}
	/* go for it if chipset is intel, or has hypertransport msi cap */
	if (pdev->vendor == PCI_VENDOR_ID_INTEL 
	    || myri10ge_hyper_msi_cap_on(pdev))
		return 1;

	/*  check if main chipset device has hypertransport msi cap */
	pdev = pci_find_slot(0, 0);
	if (pdev && myri10ge_hyper_msi_cap_on(pdev))
		return 1;

	/* default off */
	return 0;
}
#endif /* CONFIG_PCI_MSI */


#if CONFIG_64BIT
static inline void 
myri10ge_pio_copy(void *to, void *from, size_t size)
{
  register volatile uint64_t *to64;
  volatile uint64_t *from64;
  size_t i;

  to64 = (volatile uint64_t *) to;
  from64 = from;
  for (i = (size / 8); i; i--) {
	  __raw_writeq(*from64, to64);
	  to64++;
	  from64++;
  }
}
#else
static inline void
myri10ge_pio_copy(void *to, void *from, size_t size)
{
  /* Write 32-bits at a time, when possible.  This is necessary for
     the NIC's command intercept feature to work. */
  if ((((long) to | size) & 0x3) == 0) {
    uint32_t *t = (uint32_t *)to;
    uint8_t *f = (uint8_t *)from;
    
    for (; size; f+=4,size-=4,t++)
      writel (f[0] | f[1]<<8 | f[2]<<16 | f[3]<<24, t);
    return;
  }
  
  memcpy_toio(to, from, size);
}
#endif

static int 
myri10ge_send_cmd(struct myri10ge_priv *mgp, uint32_t cmd, 
		myri10ge_cmd_t *data)
{
  mcp_cmd_t *buf;
  char buf_bytes[sizeof(*buf) + 8];
  volatile mcp_cmd_response_t *response = mgp->cmd;
  volatile char *cmd_addr = mgp->sram + MYRI10GE_MCP_CMD_OFFSET;
  uint32_t dma_low, dma_high;
  int sleep_total = 0;

  /* ensure buf is aligned to 8 bytes */
  buf = (mcp_cmd_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

  buf->data0 = htonl(data->data0);
  buf->data1 = htonl(data->data1);
  buf->data2 = htonl(data->data2);
  buf->cmd = htonl(cmd);
  dma_low = MYRI10GE_LOWPART_TO_U32(mgp->cmd_bus);
  dma_high = MYRI10GE_HIGHPART_TO_U32(mgp->cmd_bus);

  buf->response_addr.low = htonl(dma_low);
  buf->response_addr.high = htonl(dma_high);
  spin_lock(&mgp->cmd_lock);
  response->result = 0xffffffff;
  mb();
  myri10ge_pio_copy((void *)cmd_addr, buf, sizeof (*buf));

  /* wait up to 2 seconds */
  for (sleep_total = 0; sleep_total <  (2 * 1000); sleep_total += 10) {
	  mb();
	  if (response->result != 0xffffffff) {
		  if (response->result == 0) {
			  data->data0 = ntohl(response->data);
			  spin_unlock(&mgp->cmd_lock);
			  return 0;
		  } else {
			  printk(KERN_ERR "myri10ge: command %d failed, result = %d\n",
				 cmd, ntohl(response->result));
			  spin_unlock(&mgp->cmd_lock);
			  return -ENXIO;
		  }
	  }
	  udelay(1000 * 10);
  }
  spin_unlock(&mgp->cmd_lock);
  printk(KERN_ERR "myri10ge: command %d timed out, result = %d\n",
	 cmd, ntohl(response->result));
  return -EAGAIN;
}

/*
 * Enable or disable periodic RDMAs from the host to make certain
 * chipsets resend dropped PCIe messages
 */

static void
myri10ge_dummy_rdma(struct myri10ge_priv *mgp, int enable)
{
	volatile uint32_t *confirm;
	char *submit;
	uint32_t buf[16];
	uint32_t dma_low, dma_high;
	int i;

	/* clear confirmation addr */
	confirm = (volatile uint32_t *)mgp->cmd;
	*confirm = 0;
	mb();

	/* send an rdma command to the PCIe engine, and wait for the
	   response in the confirmation address.  The firmware should
	   write a -1 there to indicate it is alive and well
	*/
	dma_low = MYRI10GE_LOWPART_TO_U32(mgp->cmd_bus);
	dma_high = MYRI10GE_HIGHPART_TO_U32(mgp->cmd_bus);
	
	buf[0] = htonl(dma_high); 	/* confirm addr MSW */
	buf[1] = htonl(dma_low); 	/* confirm addr LSW */
	buf[2] = htonl(0xffffffff);	/* confirm data */
	buf[3] = htonl(dma_high); 	/* dummy addr MSW */
	buf[4] = htonl(dma_low); 	/* dummy addr LSW */
	buf[5] = htonl(enable);		/* enable? */

	submit = (char *)(mgp->sram + 0xfc01c0);

	myri10ge_pio_copy(submit, &buf, sizeof (buf));
	mb();
	udelay(1000);
	mb();
	i = 0;
	while (*confirm != 0xffffffff && i < 20) {
		udelay(1000);
		i++;
	}
	if (*confirm != 0xffffffff) {
		printk(KERN_ERR "myri10ge: dummy rdma %s failed for (device %s)\n",
		       (enable ? "enable" : "disable"), pci_name(mgp->pdev));
	}
}


static int
myri10ge_load_firmware(struct myri10ge_priv *mgp)
{
	myri10ge_dummy_rdma(mgp, mgp->tx.boundary != 4096);

	return 0;
}

static int
myri10ge_update_mac_address(struct myri10ge_priv *mgp, uint8_t *addr)
{
	myri10ge_cmd_t cmd;
	int status;
	
	cmd.data0 = ((addr[0] << 24) | (addr[1] << 16) 
		     | (addr[2] << 8) | addr[3]);

	cmd.data1 = ((addr[4] << 8) | (addr[5]));

	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_SET_MAC_ADDRESS, &cmd);
	return status;
}

static int
myri10ge_change_pause(struct myri10ge_priv *mgp, int pause)
{	
	myri10ge_cmd_t cmd;
	int status;

	if (pause)
		status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_ENABLE_FLOW_CONTROL, &cmd);
	else
		status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_DISABLE_FLOW_CONTROL, &cmd);

	if (status) {
		printk(KERN_ERR "myri10ge: %s: Failed to set flow control mode\n",
		       mgp->dev->name);
		return -ENXIO;
	}
	mgp->pause = pause;
	return 0;
}

static void
myri10ge_change_promisc(struct myri10ge_priv *mgp, int promisc)
{	
	myri10ge_cmd_t cmd;
	int status;

	if (promisc)
		status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_ENABLE_PROMISC, &cmd);
	else
		status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_DISABLE_PROMISC, &cmd);

	if (status) {
		printk(KERN_ERR "myri10ge: %s: Failed to set promisc mode\n",
		       mgp->dev->name);
	}
}


static int
myri10ge_reset(struct myri10ge_priv *mgp)
{

	myri10ge_cmd_t cmd;
	int status;
	size_t bytes;

	/* try to send a reset command to the card to see if it
	   is alive */
	memset(&cmd, 0, sizeof (cmd));
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_RESET, &cmd);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed reset\n",
		       mgp->dev->name);
		return -ENXIO;
	}

	/* Now exchange information about interrupts  */

	bytes = myri10ge_max_intr_slots * sizeof (*mgp->rx_done.entry);
	memset(mgp->rx_done.entry, 0, bytes);
	cmd.data0 = (uint32_t) bytes;
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_INTRQ_SIZE, &cmd);
	cmd.data0 = MYRI10GE_LOWPART_TO_U32(mgp->rx_done.bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(mgp->rx_done.bus);
	status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_INTRQ_DMA, &cmd);


	status |= myri10ge_send_cmd(mgp,  MYRI10GE_MCP_CMD_GET_IRQ_ACK_OFFSET, &cmd);
	mgp->irq_claim = (uint32_t *)(mgp->sram + cmd.data0);

	if (!mgp->msi_enabled) {
		status |= myri10ge_send_cmd
			(mgp,  MYRI10GE_MCP_CMD_GET_IRQ_DEASSERT_OFFSET, 
			 &cmd);
		mgp->irq_deassert = (uint32_t *)(mgp->sram + cmd.data0);

	}
	status |= myri10ge_send_cmd
		(mgp, MYRI10GE_MCP_CMD_GET_INTR_COAL_DELAY_OFFSET, &cmd);
	mgp->intr_coal_delay_ptr = (uint32_t *)(mgp->sram + cmd.data0);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed set interrupt parameters\n",
		       mgp->dev->name);
		return status;
	}
	writel(htonl(mgp->intr_coal_delay), mgp->intr_coal_delay_ptr);

	/* reset mcp/driver shared state back to 0 */
	mgp->tx.req = 0;
	mgp->tx.done = 0;
	mgp->tx.pkt_start = 0;
	mgp->tx.pkt_done = 0;
	mgp->rx_big.cnt = 0;
	mgp->rx_small.cnt = 0;
	mgp->rx_done.idx = 0;
	mgp->rx_done.cnt = 0;
	status = myri10ge_update_mac_address(mgp, mgp->dev->dev_addr);
	myri10ge_change_promisc(mgp, 0);
	myri10ge_change_pause(mgp, mgp->pause);
	return status;
}

static inline void 
myri10ge_submit_8rx(mcp_kreq_ether_recv_t *dst, mcp_kreq_ether_recv_t *src)
{
	uint32_t low;

	low = src->addr_low;
	src->addr_low = 0xffffffff;
        myri10ge_pio_copy(dst, src, 8 * sizeof(*src));
	mb();
        src->addr_low = low;
	dst->addr_low = src->addr_low;
	mb();
}

static inline int
myri10ge_getbuf(myri10ge_rx_buf_t *rx, struct pci_dev *pdev, int bytes, int mcp_index, int idx)
{
        struct sk_buff *skb;
        dma_addr_t bus;
        int len, retval = 0;

	skb = dev_alloc_skb (bytes);
	if (unlikely(skb == NULL)) {
		rx->alloc_fail++;
                retval = -ENOBUFS;
                goto done;
	}
	
        /* set len so that it only covers the area we
           need mapped for DMA */
        len = bytes + MYRI10GE_MCP_ETHER_PAD;

        bus = pci_map_single(pdev, skb->data, len, PCI_DMA_FROMDEVICE);
        if (bus == INVALID_DMA_ADDR) {
                dev_kfree_skb_any(skb);
                retval = -ENXIO;
                goto done;
        }
	rx->info[idx].skb = skb;
        pci_unmap_addr_set(&rx->info[idx], bus, bus);
        pci_unmap_len_set(&rx->info[idx], len, len);
        rx->shadow[idx].addr_low = htonl(MYRI10GE_LOWPART_TO_U32(bus));
        rx->shadow[idx].addr_high = htonl(MYRI10GE_HIGHPART_TO_U32(bus));

done:
	rx->lanai[mcp_index].addr_high = rx->shadow[idx].addr_high;
	mb ();
	rx->lanai[mcp_index].addr_low = rx->shadow[idx].addr_low;
	mb ();
	return retval;
}

static inline void
myri10ge_rx_done(struct myri10ge_priv *mgp, myri10ge_rx_buf_t *rx, 
                 int bytes, int len, int csum)
{
        dma_addr_t bus;
        struct sk_buff *skb;
        int idx, mcp_index, unmap_len;

	// printf ("%s:%d\n", __FUNCTION__, __LINE__);

	mcp_index = rx->cnt & rx->lanai_mask;
        idx = mcp_index & rx->mask;
        rx->cnt++;
        
        /* save a pointer to the received skb */
        skb = rx->info[idx].skb;
        bus = pci_unmap_addr(&rx->info[idx], bus);
        unmap_len = pci_unmap_len(&rx->info[idx], len);

        /* try to replace the received skb */
        if (myri10ge_getbuf(rx, mgp->pdev, bytes, mcp_index, idx)) {
		/* drop the frame -- the old skbuf is re-cycled */
		mgp->stats.rx_dropped += 1;
		return;
	}	
	
        /* unmap the recvd skb */
	pci_unmap_single(mgp->pdev,
                         bus, unmap_len,
                         PCI_DMA_FROMDEVICE);

        /* mcp implicitly skips 1st bytes so that packet is properly
         * aligned */
        skb_reserve(skb, MYRI10GE_MCP_ETHER_PAD);

        /* set the length of the frame */
        skb_put(skb, len);

        skb->protocol = eth_type_trans(skb, mgp->dev);
        skb->dev = mgp->dev;
        if (mgp->csum_flag && (skb->protocol == ntohs(ETH_P_IP))) {
                skb->csum = ntohs((uint16_t)csum);
                skb->ip_summed = CHECKSUM_HW;
        }
        netif_rx(skb);
        mgp->dev->last_rx = jiffies;
        mgp->stats.rx_packets += 1;
        mgp->stats.rx_bytes += len;
}

static void
myri10ge_tx_done(struct myri10ge_priv *mgp, int mcp_index)
{
	struct pci_dev *pdev = mgp->pdev;
	myri10ge_tx_buf_t *tx = &mgp->tx;
	struct sk_buff *skb;
	int idx, len;

	while (tx->pkt_done != mcp_index) {
		idx = tx->done & tx->mask;
		skb = tx->info[idx].skb;

		// printf ("%s:%d\n", __FUNCTION__, __LINE__);

		/* Mark as free */
		tx->info[idx].skb = NULL;
		if (tx->info[idx].last) {
			tx->pkt_done++;
			tx->info[idx].last = 0;
		}
		tx->done++;
		len = pci_unmap_len(&tx->info[idx], len);
		pci_unmap_len_set(&tx->info[idx], len, 0);
		if (skb) {
			mgp->stats.tx_bytes += skb->len;
			mgp->stats.tx_packets++;
			dev_kfree_skb_irq(skb);
			if (len)
				pci_unmap_single(pdev, 
						 pci_unmap_addr(&tx->info[idx], bus),
						 len, PCI_DMA_TODEVICE);
		} else {
			if (len)
				pci_unmap_page(pdev, 
					       pci_unmap_addr(&tx->info[idx], bus),
					       len, PCI_DMA_TODEVICE);
		}
	}
	/* start the queue if we've stopped it */
        if (netif_queue_stopped(mgp->dev) 
            && tx->req - tx->done <= ((tx->mask+1) >> 1)) {
		mgp->wake_queue++;
                netif_wake_queue(mgp->dev);
	}
}


static inline void
myri10ge_clean_rx_done(struct myri10ge_priv *mgp)
{
	myri10ge_rx_done_t *rx_done = &mgp->rx_done;
	uint16_t length;
	uint16_t checksum;


	while (rx_done->entry[rx_done->idx].length != 0) {
		length = ntohs(rx_done->entry[rx_done->idx].length);
		rx_done->entry[rx_done->idx].length = 0;
		checksum = ntohs(rx_done->entry[rx_done->idx].checksum);
		if (length <= mgp->small_bytes)
			myri10ge_rx_done(mgp, &mgp->rx_small, 
					 mgp->small_bytes,
					 length, checksum);
		else
			myri10ge_rx_done(mgp, &mgp->rx_big, 
					 mgp->dev->mtu + ETH_HLEN,
					 length, checksum);
		rx_done->cnt++;
		rx_done->idx = rx_done->cnt & (myri10ge_max_intr_slots - 1);
	}
}



static irqreturn_t 
myri10ge_intr(int irq __unused, void *arg, struct pt_regs *regs __unused)
{
	struct myri10ge_priv *mgp = (struct myri10ge_priv *)arg;
	mcp_irq_data_t *stats = mgp->fw_stats;
	myri10ge_tx_buf_t *tx = &mgp->tx;
	myri10ge_rx_done_t *rx_done = &mgp->rx_done;
	int send_done_count;

	/* make sure the DMA has finished */
	if (unlikely(!stats->valid)) {
		return (IRQ_NONE);
	}

	if (!mgp->msi_enabled) {
		writel(0, mgp->irq_deassert);
		if (myri10ge_skip_pio_read)
			stats->valid = 0;
		mb();
	} else {
		stats->valid = 0;
	}

	do {
		/* check for transmit completes and receives */
		send_done_count = ntohl(stats->send_done_count);
		while ((send_done_count != tx->pkt_done) ||
		       (rx_done->entry[rx_done->idx].length != 0)) {
			myri10ge_tx_done(mgp, (int)send_done_count);
			myri10ge_clean_rx_done(mgp);
			send_done_count = ntohl(stats->send_done_count);
		}
	} while (*((uint8_t * volatile) &stats->valid));

	if (unlikely(stats->stats_updated)) {
		if (mgp->link_state != stats->link_up) {
			mgp->link_state = stats->link_up;
			if (mgp->link_state) {
				printk("myri10ge: %s: link up\n", 
				       mgp->dev->name);
				netif_carrier_on(mgp->dev);
			} else {
				printk("myri10ge: %s: link down\n", 
				       mgp->dev->name);
				netif_carrier_off(mgp->dev);
			}
		}
		if (mgp->rdma_tags_available !=
		    ntohl(mgp->fw_stats->rdma_tags_available)) {
			mgp->rdma_tags_available = ntohl(
				mgp->fw_stats->rdma_tags_available);
			printk("myri10ge: RDMA timed out! "
			       "%d tags left\n",
			       mgp->rdma_tags_available);
		}
		mgp->down_cnt += stats->link_down;
	}

	mb();

	writel(htonl(1), mgp->irq_claim);
	return (IRQ_HANDLED);
}

static int
myri10ge_open(struct net_device *dev)
{
	struct myri10ge_priv *mgp;
	size_t bytes;
	myri10ge_cmd_t cmd;
	int tx_ring_size, rx_ring_size;
	int tx_ring_entries, rx_ring_entries;
	int i, status, big_pow2;

	mgp = dev->priv;

	if (mgp->running != MYRI10GE_ETH_STOPPED)
		return -EBUSY;

	mgp->running = MYRI10GE_ETH_STARTING;
	status = myri10ge_reset(mgp);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed reset\n", dev->name);
		mgp->running = MYRI10GE_ETH_STOPPED;
		return -ENXIO;
	}

	/* decide what small buffer size to use.  For good TCP rx
	 * performance, it is important to not receive 1514 byte
	 * frames into jumbo buffers, as it confuses the socket buffer
	 * accounting code, leading to drops and erratic performance */

	if (dev->mtu <= ETH_DATA_LEN) {
		mgp->small_bytes = 128;			/* enough for a TCP header */
	} else {
		mgp->small_bytes = ETH_FRAME_LEN;	/* enough for an ETH_DATA_LEN frame */
	}
	/* Override the small buffer size? */
	if (myri10ge_small_bytes > 0 ) {
		mgp->small_bytes = myri10ge_small_bytes;
	}

	/* If the user sets an obscenely small MTU, adjust the small
	   bytes down to nearly nothing */
	if (mgp->small_bytes >= (dev->mtu + ETH_HLEN))
		mgp->small_bytes = 64;

	/* get ring sizes */
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_GET_SEND_RING_SIZE, &cmd);
	tx_ring_size = cmd.data0;
	status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_GET_RX_RING_SIZE, &cmd);
	rx_ring_size = cmd.data0;

	/* get the lanai pointers to the send and receive rings */

	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_GET_SEND_OFFSET, &cmd);
	mgp->tx.lanai = (mcp_kreq_ether_send_t *)(mgp->sram + cmd.data0);


	status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_GET_SMALL_RX_OFFSET, &cmd);
	mgp->rx_small.lanai = (mcp_kreq_ether_recv_t *)(mgp->sram + cmd.data0);

	status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_GET_BIG_RX_OFFSET, &cmd);
	mgp->rx_big.lanai = (mcp_kreq_ether_recv_t *)(mgp->sram + cmd.data0);

	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed to get ring sizes or locations\n",
		      dev->name);
		mgp->running = MYRI10GE_ETH_STOPPED;
		return -ENXIO;
	}

	if (mgp->mtrr >= 0) {
		mgp->tx.wc_fifo = mgp->sram + 0x200000;
		mgp->rx_small.wc_fifo = mgp->sram + 0x300000;
		mgp->rx_big.wc_fifo = mgp->sram + 0x340000;
	} else {
		mgp->tx.wc_fifo = 0;
		mgp->rx_small.wc_fifo = 0;
		mgp->rx_big.wc_fifo = 0;
	}
	
	tx_ring_entries = tx_ring_size / sizeof (mcp_kreq_ether_send_t);
	rx_ring_entries = rx_ring_size / sizeof (mcp_dma_addr_t);
	mgp->tx.lanai_mask = tx_ring_entries - 1;
	mgp->tx.mask = TX_FILL_CNT - 1;
	mgp->rx_small.lanai_mask = mgp->rx_big.lanai_mask = rx_ring_entries - 1;
	mgp->rx_small.mask = RX_SMALL_FILL_CNT - 1;
	mgp->rx_big.mask = RX_BIG_FILL_CNT - 1;
	
	/* allocate the host shadow rings */
	
	bytes = 8 + (MYRI10GE_MCP_ETHER_MAX_SEND_DESC_TSO + 4)
		* sizeof (*mgp->tx.req_list);
	assert (bytes == sizeof (preallocated->tx_req_bytes));
	mgp->tx.req_bytes = preallocated->tx_req_bytes;
	if (mgp->tx.req_bytes == NULL)
		goto abort_with_nothing;
	memset(mgp->tx.req_bytes, 0, bytes);

	/* ensure req_list entries are aligned to 8 bytes */
        mgp->tx.req_list = (mcp_kreq_ether_send_t *)
                ((unsigned long)(mgp->tx.req_bytes + 7) & ~7UL);

	bytes = (mgp->rx_small.mask + 1) * sizeof (*mgp->rx_small.shadow);
	assert (bytes == sizeof (preallocated->rx_small_shadow));
	mgp->rx_small.shadow = preallocated->rx_small_shadow;
	if (mgp->rx_small.shadow == NULL)
		goto abort_with_tx_req_bytes;
	memset(mgp->rx_small.shadow, 0, bytes);

	bytes = (mgp->rx_big.mask + 1) * sizeof (*mgp->rx_big.shadow);
	assert (bytes == sizeof (preallocated->rx_big_shadow));
	mgp->rx_big.shadow = preallocated->rx_big_shadow;
	if (mgp->rx_big.shadow == NULL)
		goto abort_with_rx_small_shadow;
	memset(mgp->rx_big.shadow, 0, bytes);

	/* allocate the host info rings */

	bytes = (mgp->tx.mask + 1) * sizeof (*mgp->tx.info);
	assert (bytes == sizeof (preallocated->tx_info));
	mgp->tx.info = preallocated->tx_info;
	if (mgp->tx.info == NULL)
		goto abort_with_rx_big_shadow;
	memset(mgp->tx.info, 0, bytes);
	
	bytes = (mgp->rx_small.mask + 1) * sizeof (*mgp->rx_small.info);
	assert (bytes == sizeof (preallocated->rx_small_info));
	mgp->rx_small.info = preallocated->rx_small_info;
	if (mgp->rx_small.info == NULL)
		goto abort_with_tx_info;
	memset(mgp->rx_small.info, 0, bytes);

	bytes = (mgp->rx_big.mask + 1) * sizeof (*mgp->rx_big.info);
	assert (bytes == sizeof (preallocated->rx_big_info));
	mgp->rx_big.info = preallocated->rx_big_info;
	if (mgp->rx_big.info == NULL)
		goto abort_with_rx_small_info;
	memset(mgp->rx_big.info, 0, bytes);

	/* Fill the receive rings */
	for (i = 0; i <= mgp->rx_small.mask; i++) {
		status = myri10ge_getbuf(&mgp->rx_small, mgp->pdev, 
					 mgp->small_bytes, i, i);
		if (status) {
			printk(KERN_ERR "myri10ge: %s: alloced only %d small bufs\n",
			       dev->name, i);
			goto abort_with_rx_small_ring;
		}
	}
	mgp->rx_small.cnt = mgp->rx_small.mask + 1;

	for (i = 0; i <= mgp->rx_big.mask; i++) {
		status = myri10ge_getbuf(&mgp->rx_big, mgp->pdev,
					 dev->mtu + ETH_HLEN, i, i);
		if (status) {
			printk(KERN_ERR "myri10ge: %s: alloced only %d big bufs\n",
			       dev->name, i);
			goto abort_with_rx_big_ring;
		}
	}
	mgp->rx_big.cnt = mgp->rx_big.mask + 1;

	/* Firmware needs the big buff size as a power of 2.  Lie and
	   tell him the buffer is larger, because we only use 1
	   buffer/pkt, and the mtu will prevent overruns */
	big_pow2 = dev->mtu + ETH_HLEN + MYRI10GE_MCP_ETHER_PAD;
	while ((big_pow2 & (big_pow2 - 1)) != 0)
		big_pow2++;

	/* now give firmware buffers sizes, and MTU */
        cmd.data0 = dev->mtu + ETH_HLEN;
        status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_MTU, &cmd);
        cmd.data0 = mgp->small_bytes;
        status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_SMALL_BUFFER_SIZE, &cmd);
	cmd.data0 = big_pow2;
        status |= myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_BIG_BUFFER_SIZE, &cmd);	
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't set buffer sizes\n",
		       dev->name);
		goto abort_with_rx_big_ring;
	}

	cmd.data0 = MYRI10GE_LOWPART_TO_U32(mgp->fw_stats_bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(mgp->fw_stats_bus);
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_SET_STATS_DMA, &cmd);
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't set stats DMA\n",
		       dev->name);
		goto abort_with_rx_big_ring;
	}

	mgp->link_state = -1;
	mgp->rdma_tags_available = 15;
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_ETHERNET_UP, &cmd);
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't bring up link\n",
		       dev->name);
		goto abort_with_rx_big_ring;
	}

	mgp->wake_queue = 0;
	mgp->stop_queue = 0;
	mgp->running = MYRI10GE_ETH_RUNNING;
	netif_wake_queue(dev);
	return 0;

abort_with_rx_big_ring:
	for (i = 0; i <= mgp->rx_big.mask; i++) {
		if (mgp->rx_big.info[i].skb != NULL) 
			dev_kfree_skb_any(mgp->rx_big.info[i].skb);
		if (pci_unmap_len(&mgp->rx_big.info[i], len)) {
			pci_unmap_single(mgp->pdev, 
					 pci_unmap_addr(&mgp->rx_big.info[i], bus),
					 pci_unmap_len(&mgp->rx_big.info[i], len),
					 PCI_DMA_FROMDEVICE);
		}
	}

abort_with_rx_small_ring:
	for (i = 0; i <= mgp->rx_small.mask; i++) {
		if (mgp->rx_small.info[i].skb != NULL) 
			dev_kfree_skb_any(mgp->rx_small.info[i].skb);
		if (pci_unmap_len(&mgp->rx_small.info[i], len)) {
			pci_unmap_single(mgp->pdev, 
					 pci_unmap_addr(&mgp->rx_small.info[i], bus),
					 pci_unmap_len(&mgp->rx_small.info[i], len),
					 PCI_DMA_FROMDEVICE);
		}
	}
	kfree(mgp->rx_big.info);

abort_with_rx_small_info:
	kfree(mgp->rx_small.info);

abort_with_tx_info:
	kfree(mgp->tx.info);

abort_with_rx_big_shadow:
	kfree(mgp->rx_big.shadow);

abort_with_rx_small_shadow:
	kfree(mgp->rx_small.shadow);

abort_with_tx_req_bytes:
	kfree(mgp->tx.req_bytes);
	mgp->tx.req_bytes = NULL;
	mgp->tx.req_list = NULL;

abort_with_nothing:
	mgp->running = MYRI10GE_ETH_STOPPED;
	return -ENOMEM;

}
static int
myri10ge_close(struct net_device *dev)
{
	struct myri10ge_priv *mgp;
	struct sk_buff *skb;
	myri10ge_tx_buf_t *tx;
	int status, i, old_down_cnt, len, idx;
	myri10ge_cmd_t cmd;



	mgp = dev->priv;

	if (mgp->running != MYRI10GE_ETH_RUNNING)
		return 0;

	if (mgp->tx.req_bytes == NULL)
		return 0;

	mgp->running = MYRI10GE_ETH_STOPPING;
	netif_carrier_off(dev);
	netif_stop_queue(dev);
	old_down_cnt = mgp->down_cnt;
	mb();
	status = myri10ge_send_cmd(mgp, MYRI10GE_MCP_CMD_ETHERNET_DOWN, &cmd);
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't bring down link\n",
		       dev->name);
	}
	sleep (1);		/* FIXME */
	netif_tx_disable(dev);

	for (i = 0; i <= mgp->rx_big.mask; i++) {
		if (mgp->rx_big.info[i].skb != NULL) 
			dev_kfree_skb_any(mgp->rx_big.info[i].skb);
		if (pci_unmap_len(&mgp->rx_big.info[i], len)) {
			pci_unmap_single(mgp->pdev, 
					 pci_unmap_addr(&mgp->rx_big.info[i], bus),
					 pci_unmap_len(&mgp->rx_big.info[i], len),
					 PCI_DMA_FROMDEVICE);
		}
	}

	for (i = 0; i <= mgp->rx_small.mask; i++) {
		if (mgp->rx_small.info[i].skb != NULL) 
			dev_kfree_skb_any(mgp->rx_small.info[i].skb);
		if (pci_unmap_len(&mgp->rx_small.info[i], len)) {
			pci_unmap_single(mgp->pdev, 
					 pci_unmap_addr(&mgp->rx_small.info[i], bus),
					 pci_unmap_len(&mgp->rx_small.info[i], len),
					 PCI_DMA_FROMDEVICE);
		}
	}

	tx = &mgp->tx;
	while (tx->done != tx->req) {
		idx = tx->done & tx->mask;
		skb = tx->info[idx].skb;
		
		/* Mark as free */
		tx->info[idx].skb = NULL;
		tx->done++;
		len = pci_unmap_len(&tx->info[idx], len);
		pci_unmap_len_set(&tx->info[idx], len, 0);
		if (skb) {
			mgp->stats.tx_dropped++;
			dev_kfree_skb_irq(skb);
			if (len)
				pci_unmap_single(mgp->pdev, 
						 pci_unmap_addr(&tx->info[idx], bus),
						 len, PCI_DMA_TODEVICE);
		} else {
			if (len)
				pci_unmap_page(mgp->pdev, 
					       pci_unmap_addr(&tx->info[idx], bus),
					       len, PCI_DMA_TODEVICE);
		}
	}
	kfree(mgp->rx_big.info);

	kfree(mgp->rx_small.info);

	kfree(mgp->tx.info);

	kfree(mgp->rx_big.shadow);

	kfree(mgp->rx_small.shadow);

	kfree(mgp->tx.req_bytes);
	mgp->tx.req_bytes = NULL;
	mgp->tx.req_list = NULL;
	mgp->running = MYRI10GE_ETH_STOPPED;
	return 0;
}

/* copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy 
   backwards one at a time and handle ring wraps */

static inline void 
myri10ge_submit_req_backwards(myri10ge_tx_buf_t *tx, 
			    mcp_kreq_ether_send_t *src, int cnt)
{
        int idx, starting_slot;
        starting_slot = tx->req;
        while (cnt > 1) {
                cnt--;
                idx = (starting_slot + cnt) & tx->lanai_mask;
                myri10ge_pio_copy(&tx->lanai[idx],
				&src[cnt], sizeof(*src));
                mb();
        }
}

/*
 * copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy
 * at most 32 bytes at a time, so as to avoid involving the software
 * pio handler in the nic.   We re-write the first segment's flags
 * to mark them valid only after writing the entire chain 
 */

static inline void 
myri10ge_submit_req(myri10ge_tx_buf_t *tx, mcp_kreq_ether_send_t *src, 
                  int cnt)
{
        int idx, mcp_index, i;
        uint32_t *src_ints, *dst_ints;
        mcp_kreq_ether_send_t *srcp, *dstp, *dst;
	uint8_t last_flags;

        mcp_index = tx->req & tx->lanai_mask;
        idx = mcp_index & tx->mask;

	last_flags = src->flags;
	src->flags = 0;
        mb();
        dst = dstp = &tx->lanai[mcp_index];
        srcp = src;

        if ((idx + cnt) <= (tx->mask + 1)) {
                for (i = 0; i < (cnt - 1); i += 2) {
                        myri10ge_pio_copy(dstp, srcp, 2 * sizeof(*src));
                        mb(); /* force write every 32 bytes */
                        srcp += 2;
                        dstp += 2;
                }
        } else {
                /* submit all but the first request, and ensure 
                   that it is submitted below */
                myri10ge_submit_req_backwards(tx, src, cnt);
                i = 0;
        }
        if (i < cnt) {
                /* submit the first request */
                myri10ge_pio_copy(dstp, srcp, sizeof(*src));
                mb(); /* barrier before setting valid flag */
        }

        /* re-write the last 32-bits with the valid flags */
        src->flags = last_flags;
        src_ints = (uint32_t *)src;
        src_ints+=3;
        dst_ints = (uint32_t *)dst;
        dst_ints+=3;
        *dst_ints =  *src_ints;
        tx->req += cnt;
        mb();
}

static inline void
myri10ge_submit_req_wc(myri10ge_tx_buf_t *tx,
		     mcp_kreq_ether_send_t *src, int cnt)
{
    tx->req += cnt;
    mb();
    while (cnt >= 4) {
	    myri10ge_pio_copy((char *)tx->wc_fifo, src, 64);
	    mb();
	    src += 4;
	    cnt -= 4;
    }
    if (cnt > 0) {
	    /* pad it to 64 bytes.  The src is 64 bytes bigger than it
	       needs to be so that we don't overrun it */
	    myri10ge_pio_copy((char *)tx->wc_fifo + (cnt<<18), src, 64);
	    mb();
    }
}

#ifdef NETIF_F_TSO
static unsigned long
myri10ge_tcpend(struct sk_buff *skb)
{
	struct iphdr *ip;
	int iphlen, tcplen;
	struct tcphdr *tcp;

	ip = (struct iphdr *)((char *)skb->data + 14);
	iphlen = ip->ihl << 2;
	tcp = (struct tcphdr *)((char *)ip + iphlen);
	tcplen = tcp->doff << 2;
	return (tcplen + iphlen + 14);
}
#endif

/*
 * Transmit a packet.  We need to split the packet so that a single
 * segment does not cross myri10ge->tx.boundary, so this makes segment
 * counting tricky.  So rather than try to count segments up front, we
 * just give up if there are too few segments to hold a reasonably
 * fragmented packet currently available.  If we run
 * out of segments while preparing a packet for DMA, we just linearize
 * it and try again.
 */

static int
myri10ge_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct myri10ge_priv *mgp = dev->priv;
	mcp_kreq_ether_send_t *req;
	myri10ge_tx_buf_t *tx = &mgp->tx;
	struct skb_frag_struct *frag;
	dma_addr_t bus;
	uint32_t low, high_swapped;
	int len;
	int idx, last_idx, avail, frag_cnt, frag_idx, count, mss, max_segments;
	uint16_t pseudo_hdr_offset;
	int cum_len, seglen, boundary, rdma_count;
	uint8_t cksum_offset, flags, odd_flag;

again:
	req = tx->req_list;
	avail = tx->mask + 1 - (tx->req - tx->done);
	
	mss = 0;
	max_segments = MYRI10GE_MCP_ETHER_MAX_SEND_DESC;

#ifdef NETIF_F_TSO
	if (skb->len > (dev->mtu + ETH_HLEN)) {
		mss = skb_shinfo(skb)->tso_size;
		if (unlikely(mss == 0)) {
			printk(KERN_ERR "myri10ge: %s: TSO but mss==0?!?!?\n",
			       mgp->dev->name);
			goto drop;
		}
		max_segments = MYRI10GE_MCP_ETHER_MAX_SEND_DESC_TSO;	
	}
#endif /*NETIF_F_TSO */

	if ((unlikely(avail < max_segments))) {
		/* we are out of transmit resources */
		mgp->stop_queue++;
		netif_stop_queue(dev);
		return 1;
	}

	/* Setup checksum offloading, if needed */
	cksum_offset = 0;
	pseudo_hdr_offset = 0;
	odd_flag = 0;
	flags = (MYRI10GE_MCP_ETHER_FLAGS_NO_TSO |
		 MYRI10GE_MCP_ETHER_FLAGS_FIRST);
	if (likely(skb->ip_summed == CHECKSUM_HW)) {
		cksum_offset = (skb->h.raw - skb->data);
		pseudo_hdr_offset = htons((skb->h.raw + skb->csum) - skb->data);
		odd_flag = MYRI10GE_MCP_ETHER_FLAGS_ALIGN_ODD;
		flags |= MYRI10GE_MCP_ETHER_FLAGS_CKSUM;
	}

	cum_len = 0;

	/* Mark small packets, and pad out tiny packets */
	if (skb->len <= MYRI10GE_MCP_ETHER_SEND_SMALL_SIZE) {
		flags |= MYRI10GE_MCP_ETHER_FLAGS_SMALL;

		/* pad frames to at least ETH_ZLEN bytes */
		if (unlikely(skb->len < ETH_ZLEN)) {
			skb = skb_padto(skb, ETH_ZLEN);
			if (skb == NULL) {
				/* The packet is gone, so we must
				   return 0 */
				mgp->stats.tx_dropped += 1;
				return 0;
			}
			/* adjust the len to account for the zero pad
			   so that the nic can know how long it is */
			skb->len = ETH_ZLEN;
		}
	}
#ifdef NETIF_F_TSO
	else if (mss) { /* TSO */
		/* this removes any CKSUM flag from before */
		flags = (MYRI10GE_MCP_ETHER_FLAGS_TSO_HDR |
			 MYRI10GE_MCP_ETHER_FLAGS_FIRST);

		/* negative cum_len signifies to the
		   send loop that we are still in the
		   header portion of the TSO packet.
		   TSO header must be at most 134 bytes long */
		cum_len = -myri10ge_tcpend(skb);

		/* for TSO, pseudo_hdr_offset holds mss.
		   The firmware figures out where to put
		   the checksum by parsing the header. */
		pseudo_hdr_offset = htons(mss);
	}
#endif /*NETIF_F_TSO */

	/* map the skb for DMA */
	len = skb->len - skb->data_len;
	idx = tx->req & tx->mask;
	tx->info[idx].skb = skb;
	bus = pci_map_single(mgp->pdev, skb->data, len, PCI_DMA_TODEVICE);
	pci_unmap_addr_set(&tx->info[idx], bus, bus);
	pci_unmap_len_set(&tx->info[idx], len, len);

	frag_cnt = skb_shinfo(skb)->nr_frags;
	frag_idx = 0;
	count = 0;
	rdma_count = 0;

	/* "rdma_count" is the number of RDMAs belonging to the
	   current packet BEFORE the current send request. For
	   non-TSO packets, this is equal to "count".
	   For TSO packets, rdma_count needs to be reset
	   to 0 after a segment cut.

	   The rdma_count field of the send request is
	   the number of RDMAs of the packet starting at
	   that request. For TSO send requests with one ore more cuts
	   in the middle, this is the number of RDMAs starting
	   after the last cut in the request. All previous
	   segments before the last cut implicitly have 1 RDMA.

	   Since the number of RDMAs is not known beforehand,
	   it must be filled-in retroactively - after each
	   segmentation cut or at the end of the entire packet.
	*/

	while (1) { 
		/* Break the SKB or Fragment up into pieces which
		   do not cross mgp->tx.boundary */
		low = MYRI10GE_LOWPART_TO_U32(bus);
		high_swapped = htonl(MYRI10GE_HIGHPART_TO_U32(bus));
		while (len) {
			uint8_t flags_next;
			int cum_len_next;

			if (unlikely(count == max_segments))
				goto abort_linearize;

			boundary = (low + tx->boundary) & ~(tx->boundary - 1);
			seglen = boundary - low;
			if (seglen > len)
				seglen = len;
			flags_next = flags & ~MYRI10GE_MCP_ETHER_FLAGS_FIRST;
			cum_len_next = cum_len + seglen;
#ifdef NETIF_F_TSO
			if (mss) { /* TSO */
				(req-rdma_count)->rdma_count = rdma_count + 1;

				if (likely(cum_len>=0)) { /* payload */
					int next_is_first, chop;

					chop = (cum_len_next>mss);
					cum_len_next = cum_len_next % mss;
					next_is_first = (cum_len_next==0);
					flags |= chop *
						MYRI10GE_MCP_ETHER_FLAGS_TSO_CHOP;
					flags_next |= next_is_first *
						MYRI10GE_MCP_ETHER_FLAGS_FIRST;
					rdma_count |= -(chop | next_is_first);
					rdma_count += chop & !next_is_first;
				} else if (likely(cum_len_next>=0)) { /* header ends */
					int small;

					rdma_count = -1;
					cum_len_next = 0;
					seglen = -cum_len;
					small = (mss<=MYRI10GE_MCP_ETHER_SEND_SMALL_SIZE);
					flags_next = MYRI10GE_MCP_ETHER_FLAGS_TSO_PLD |
						MYRI10GE_MCP_ETHER_FLAGS_FIRST |
						(small * MYRI10GE_MCP_ETHER_FLAGS_SMALL);
				}
			}
#endif /* NETIF_F_TSO */
			req->addr_high = high_swapped;
			req->addr_low = htonl(low);
			req->pseudo_hdr_offset = pseudo_hdr_offset;
			req->pad = 0; /* complete solid 16-byte block; does this matter? */
			req->rdma_count = 1;
			req->length = htons(seglen);
			req->cksum_offset = cksum_offset;
			req->flags = flags | ((cum_len & 1) * odd_flag);

			low += seglen;
			len -= seglen;
			cum_len = cum_len_next;
			flags = flags_next;
			req++;
			count++;
			rdma_count++;
			if (unlikely(cksum_offset > seglen))
				cksum_offset -= seglen;
			else
				cksum_offset = 0;
		}
		if (frag_idx == frag_cnt)
			break;

		/* map next fragment for DMA */
		idx = (count + tx->req) & tx->mask;
		frag = &skb_shinfo(skb)->frags[frag_idx];
		frag_idx++;
		len = frag->size;
		bus = pci_map_page(mgp->pdev, frag->page, frag->page_offset,
				   len, PCI_DMA_TODEVICE);
		pci_unmap_addr_set(&tx->info[idx], bus, bus);
		pci_unmap_len_set(&tx->info[idx], len, len);
	}

	(req-rdma_count)->rdma_count = rdma_count;
#ifdef NETIF_F_TSO
	if (mss) {
		do {
			req--;
			req->flags |= MYRI10GE_MCP_ETHER_FLAGS_TSO_LAST;
		} while (!(req->flags & (MYRI10GE_MCP_ETHER_FLAGS_TSO_CHOP |
					 MYRI10GE_MCP_ETHER_FLAGS_FIRST)));
	}
#endif
	idx = ((count - 1) + tx->req) & tx->mask;
	tx->info[idx].last = 1;
	if (tx->wc_fifo == NULL)
		myri10ge_submit_req(tx, tx->req_list, count);
	else
		myri10ge_submit_req_wc(tx, tx->req_list, count);
	tx->pkt_start++;
	if ((avail - count) < MYRI10GE_MCP_ETHER_MAX_SEND_DESC) {
		mgp->stop_queue++;
		netif_stop_queue(dev);
	}
	dev->trans_start = jiffies;
	return 0;


abort_linearize:
	/* Free any DMA resources we've alloced and clear out the skb
	 slot so as to not trip up assertions, and to avoid a
	 double-free if linearizing fails */

	last_idx = (idx + 1) & tx->mask;
	idx = tx->req & tx->mask;
	tx->info[idx].skb = NULL;
	do {
		len = pci_unmap_len(&tx->info[idx], len);
		if (len) {
			if (tx->info[idx].skb != NULL) {
				pci_unmap_single(mgp->pdev,
						 pci_unmap_addr(&tx->info[idx], bus),
						 len, PCI_DMA_TODEVICE);
			} else {
				pci_unmap_page(mgp->pdev,
					       pci_unmap_addr(&tx->info[idx], bus),
					       len, PCI_DMA_TODEVICE);
			}		
			pci_unmap_len_set(&tx->info[idx], len, 0);
			tx->info[idx].skb = NULL;
		}
		idx = (idx + 1) & tx->mask;
	} while (idx != last_idx);
	if (skb_shinfo(skb)->tso_size) {
		printk(KERN_ERR "myri10ge: %s: TSO but wanted to linearize?!?!?\n",
		       mgp->dev->name);
		goto drop;
	}

	if (skb_linearize(skb, GFP_ATOMIC)) {
		goto drop;
	}
	mgp->tx_linearized++;
	goto again;

drop:
	dev_kfree_skb_any(skb);
	mgp->stats.tx_dropped += 1;
	return 0;


}

/*
 * The Lanai Z8E PCI-E interface achieves higher Read-DMA throughput
 * when the PCI-E Completion packets are aligned on an 8-byte
 * boundary.  Some PCI-E chip sets always align Completion packets; on
 * the ones that do not, the alignment can be enforced by enabling
 * ECRC generation (if supported).
 *
 * When PCI-E Completion packets are not aligned, it is actually more
 * efficient to limit Read-DMA transactions to 2KB, rather than 4KB.
 *
 * If the driver can neither enable ECRC nor verify that it has
 * already been enabled, then it must use a firmware image which works
 * around unaligned completion packets (ethp_z8e.dat), and it should
 * also ensure that it never gives the device a Read-DMA which is
 * larger than 2KB by setting the tx.boundary to 2KB.  If ECRC is
 * enabled, then the driver should use the aligned (eth_z8e.dat)
 * firmware image, and set tx.boundary to 4KB.
 */

static void
myri10ge_select_firmware(struct myri10ge_priv *mgp)
{
	mgp->tx.boundary = 2048;
	mgp->fw_name = myri10ge_fw_unaligned;
}
static int
myri10ge_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct myri10ge_priv *mgp;
	size_t bytes;
	int i;
	int status = -ENXIO;
	int cap;
	u16 val;
	

	dev_info(dev, "myri10ge_probe: called\n");
	netdev = alloc_etherdev(sizeof(*mgp));
	if (netdev == NULL) {
		dev_err(dev, "Could not allocate ethernet device\n");
		return -ENOMEM;
	}

	mgp = netdev_priv(netdev);
	memset(mgp, 0, sizeof (*mgp));
	mgp->dev = netdev;
	mgp->pdev = pdev;
	mgp->csum_flag = MYRI10GE_MCP_ETHER_FLAGS_CKSUM;
	mgp->pause = myri10ge_flow_control;
	mgp->intr_coal_delay = myri10ge_intr_coal_delay;

	spin_lock_init(&mgp->cmd_lock);
	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "myri10ge: pci_enable_device call failed (device %s)\n",
		       pci_name(pdev));
		status = -ENODEV;
		goto abort_with_netdev;
	}
	myri10ge_select_firmware(mgp);

	/* Find the vendor-specific cap so we can check
	   the reboot register later on */
	mgp->vendor_specific_offset 
		= pci_find_capability(pdev, PCI_CAP_ID_VNDR);
	(void) cap;		/* minimize diff against Linux driver */
	(void) val;		/* minimize diff against Linux driver */
	(void) ent;		/* minimize diff against Linux driver */
	if (0) goto abort_with_ioremap;/* minimize diff against Linux driver */
	if (0) goto abort_with_wc;/* minimize diff against Linux driver */
	adjust_pci_device (pdev);
	assert (sizeof (*mgp->cmd) == sizeof (preallocated->cmd));
	mgp->cmd = (mcp_cmd_response_t *) preallocated->cmd;
	mgp->cmd_bus = virt_to_bus (mgp->cmd);
	if (mgp->cmd == NULL) {
		goto abort_with_netdev;
	}

	assert (sizeof (*mgp->fw_stats) < sizeof (preallocated->fw_stats));
	mgp->fw_stats = (mcp_irq_data_t *) preallocated->fw_stats;
	mgp->fw_stats_bus = virt_to_bus (mgp->fw_stats);
	if (mgp->fw_stats == NULL) {
		goto abort_with_cmd;
	}

 
	netdev->name = "eth%d";
	mgp->board_span = 16*1024*1024;	/* FIXME: magic */
	pcibios_read_config_dword (pdev->bus, pdev->devfn,
				   PCI_BASE_ADDRESS_0, &mgp->iomem_base);
	mgp->iomem_base &= PCI_BASE_ADDRESS_MEM_MASK;
	mgp->mtrr = -1;
#ifdef CONFIG_MTRR
	mgp->mtrr = mtrr_add(mgp->iomem_base, mgp->board_span,
			     MTRR_TYPE_WRCOMB, 1);
#endif
	mgp->sram = phys_to_virt (mgp->iomem_base);
	mgp->mcp_hdr
		= ((struct mcp_gen_header *)
		   (mgp->sram + ntohl (*(uint32_t *)(mgp->sram + MCP_HEADER_PTR_OFFSET))));
	mgp->sram_size = ntohl (mgp->mcp_hdr->string_specs);
	memcpy (mgp->eeprom_strings,
		(char *)mgp->sram + mgp->sram_size,
		MYRI10GE_EEPROM_STRINGS_SIZE);
	string_specs_to_mac_addr (mgp->eeprom_strings, mgp->mac_addr);
 
        for (i = 0; i < 6; i++) {
                netdev->dev_addr[i]= mgp->mac_addr[i];
        }
	/* allocate rx done ring */
	bytes = myri10ge_max_intr_slots * sizeof (*mgp->rx_done.entry);
	assert (bytes == sizeof (preallocated->rx_done_entry));
	mgp->rx_done.entry = preallocated->rx_done_entry;
	mgp->rx_done.bus = virt_to_bus (mgp->rx_done.entry);
	if (mgp->rx_done.entry == NULL)
		goto abort_with_ioremap;
	memset(mgp->rx_done.entry, 0, bytes);

	status = myri10ge_load_firmware(mgp);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed to load firmware\n",
		       pci_name(pdev));
		goto abort_with_rx_done;
	}

	status = myri10ge_reset(mgp);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed reset\n",
		       pci_name(pdev));
		goto abort_with_firmware;
	}

#ifdef CONFIG_PCI_MSI
	if (myri10ge_use_msi(pdev)) {
		status = pci_enable_msi(pdev);
		if (status != 0) {
			printk(KERN_ERR "myri10ge: Error %d setting up MSI on device %s, falling back to xPIC\n",
			       status, pci_name(pdev));

		} else {
			mgp->msi_enabled = 1;
		}
	}
#endif
	status = request_irq(pdev->irq, myri10ge_intr, SA_SHIRQ,
			     netdev->name, mgp);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed to allocate IRQ\n",
		       pci_name(pdev));
		goto abort_with_firmware;
	}

	pci_set_drvdata(pdev, mgp);
	if ((myri10ge_initial_mtu + ETH_HLEN) > MYRI10GE_MAX_ETHER_MTU)
		myri10ge_initial_mtu = MYRI10GE_MAX_ETHER_MTU - ETH_HLEN;
	if ((myri10ge_initial_mtu + ETH_HLEN) < 68)
		myri10ge_initial_mtu = 68;
	netdev->mtu = myri10ge_initial_mtu;
	if (0) {
		goto abort_with_irq;
	}


	printk("myri10ge: %s: %s IRQ %d, tx boundary %d, firmware %s, WC %s\n", 
	       netdev->name,  (mgp->msi_enabled ? "MSI" : "xPIC"),  
	       pdev->irq, mgp->tx.boundary, mgp->fw_name,
	       (mgp->mtrr >=0 ? "Enabled" : "Disabled"));

	return 0;

abort_with_irq:
        free_irq(pdev->irq, mgp);
#ifdef CONFIG_PCI_MSI
	if (mgp->msi_enabled)
		pci_disable_msi(pdev);
#endif	

abort_with_firmware:
	myri10ge_dummy_rdma(mgp, 0);

abort_with_rx_done:
	bytes = myri10ge_max_intr_slots * sizeof (*mgp->rx_done.entry);
	pci_free_consistent(pdev, bytes, mgp->rx_done.entry, mgp->rx_done.bus);
	
abort_with_ioremap:
	iounmap((char *)mgp->sram);

abort_with_wc:
#ifdef CONFIG_MTRR
	if (mgp->mtrr >= 0)
		mtrr_del(mgp->mtrr, mgp->iomem_base, mgp->board_span);
#endif
	pci_free_consistent(pdev, sizeof (*mgp->fw_stats), 
			    mgp->fw_stats, mgp->fw_stats_bus);

abort_with_cmd:
	pci_free_consistent(pdev, sizeof (*mgp->cmd), mgp->cmd, mgp->cmd_bus);

abort_with_netdev:

	free_netdev(netdev);
	return status;
}

/****************************************************************
 * myri10ge_remove
 *
 * Does what is necessary to shutdown one Myrinet device. Called
 *   once for each Myrinet card by the kernel when a module is
 *   unloaded.
 ****************************************************************/

static void
myri10ge_remove(struct pci_dev *pdev)
{
	struct myri10ge_priv *mgp;
	struct net_device *netdev;
	size_t bytes;

	mgp = (struct myri10ge_priv *)pci_get_drvdata(pdev);
	if (mgp == NULL)
		return;

	flush_scheduled_work();
	netdev = mgp->dev;
	unregister_netdev(netdev);
        free_irq(pdev->irq, mgp);
#ifdef CONFIG_PCI_MSI
	if (mgp->msi_enabled)
		pci_disable_msi(pdev);
#endif	

	myri10ge_dummy_rdma(mgp, 0);


	bytes = myri10ge_max_intr_slots * sizeof (*mgp->rx_done.entry);
	pci_free_consistent(pdev, bytes, mgp->rx_done.entry, mgp->rx_done.bus);
	

	iounmap((char *)mgp->sram);

#ifdef CONFIG_MTRR
	if (mgp->mtrr >= 0)
		mtrr_del(mgp->mtrr, mgp->iomem_base, mgp->board_span);
#endif
	pci_free_consistent(pdev, sizeof (*mgp->fw_stats), 
			    mgp->fw_stats, mgp->fw_stats_bus);

	pci_free_consistent(pdev, sizeof (*mgp->cmd), mgp->cmd, mgp->cmd_bus);

	free_netdev(netdev);
	pci_set_drvdata(pdev,0);
}

/****************
 * HACK to suppress "defined but not used" warnings in Linux code without
 * patching it.
 ****************/

void suppress_defined_but_not_used (void) 
{
	(void) myri10ge_fw_name;
	(void) myri10ge_force_firmware;
	(void) myri10ge_skb_cross_4k;
	suppress_defined_but_not_used ();
} 
 
/****************************************************************
 * Etherboot entry points
 ****************************************************************/ 
 
/****************
 * POLL - Wait for a frame
 ****************/
 
static int 
myri10ge_etherboot_poll (struct nic *nic, int retrieve)
{ 
	struct sk_buff *recv;
	struct myri10ge_priv *mgp = nic->priv_data;
  
	/* Process any pending interrupts. */
 
	myri10ge_intr (nic->irqno, mgp, 0);
  
	/* Return if no receive is pending. */
 
	recv = rx_first;
	if (!recv) {
		return 0;
	}
 
	/* Return if only polling. */
  
	if (!retrieve) return 1;
	// printf ("%s:%d\n", __FUNCTION__, __LINE__);
	
	/* Copy the received packet to nic->packet, and claim the packet. */
 
	memcpy (nic->packet, recv->data, recv->len);
	nic->packetlen = recv->len;
  
	/* Remove the packet from the receive queue, and free it. */
 
	rx_first = rx_first->next;
	if (!rx_first)
		rx_last = (struct sk_buff *)&rx_first;
	dev_kfree_skb_any (recv);
	
	return 1;
}  
 
/****************
 * TRANSMIT - Transmit a frame
 ****************/
 
static void 
myri10ge_etherboot_transmit (struct nic *nic,
			     const char *d,	/* Destination */
			     unsigned int type,	/* Type */
			     unsigned int size,	/* size */
			     const char *p)	/* Packet */
{
	struct myri10ge_priv *mgp = nic->priv_data;
	struct sk_buff *send;
	unsigned short ntype;
 
	// printf ("%s %d\n", __FUNCTION__, __LINE__);
	
	/* Allocate a send buffer. */
 
	do {
		myri10ge_etherboot_poll (nic, 0);
	 
		/* Drop sends when link is down or transmits are disabled. */
		
		if ((preallocated_net_device.state & (NETDEV_TX_DISABLED
						      | NETDEV_TX_STOPPED
						      | NETDEV_CARRIER_OFF))) {
			mgp->stats.tx_dropped++;
			printf ("dropped (disabled %d)\n",
				preallocated_net_device.state);
			return;
		}
		
		send = free_tx_skb;
	} while (!send);
	free_tx_skb = send->next;
		
	/* fill it. */
 
	// printf ("%! %! %hx ...\n", d, nic->node_addr, type);
	skb_reserve (send, MYRI10GE_MCP_ETHER_PAD);
	memcpy (skb_put (send, 6), d, 6);
	memcpy (skb_put (send, 6), nic->node_addr, 6);
	ntype = htons (type);
	memcpy (skb_put (send, 2), &ntype, 2);
	memcpy (skb_put (send, size), p, size);
	 
	/* send it. */
	
	myri10ge_xmit (send, &preallocated_net_device);
 
	return;
} 
 
/****************
 * DISABLE - Turn off ethernet interface (opposite of probe())
 ****************/
 
static void myri10ge_etherboot_disable (struct dev *dev)
{ 
	struct nic *nic = (struct nic *)dev;
	struct myri10ge_priv *mgp = (struct myri10ge_priv *)nic->priv_data;
	
	// printf ("%s %d\n", __FUNCTION__, __LINE__);
	myri10ge_close (mgp->dev);
	myri10ge_remove (mgp->pdev);
	return;
} 
 
/****************
 * IRQ - Enable, Disable, or Force interrupts
 ****************/
 
static void myri10ge_etherboot_irq(struct nic *nic __unused,
				   irq_action_t action)
{ 
	// printf ("%s %d\n", __FUNCTION__, __LINE__);
	switch ( action ) {
	case DISABLE :
		printf ("disable\n");
		break;
	case ENABLE :
		printf ("enable\n");
		break;
	case FORCE :
		printf ("force\n");
		break;
	} 
}

/****************
 * PROBE - Look for an adapter, this routine is visible to the outside
 ****************/

static int myri10ge_etherboot_probe(struct dev *dev, struct pci_device *pci)
{ 
	struct myri10ge_priv *mgp = &_mgp;
	struct nic *nic = (struct nic *)dev;
	 
	/* Preallocate large buffers from the shared pool. */
	
	preallocate ();
	
	/* Call myri10ge_probe() and myri10ge_open(), which were stolen from
	   the Linux driver and minimally modified. */
  
	if (myri10ge_probe (pci, 0)) {
		printf ("Failed to probe NIC.\n");
		return 0;
	} 
	if (myri10ge_open (&preallocated_net_device)) {
		printf ("Failed to open NIC\n");
		return 0;
	} 
	 
	/* store NIC parameters -- per skel.c */
	nic->ioaddr = pci->ioaddr & ~3;
	nic->irqno = pci->irq;
	/* point to NIC specific routines */
	dev->disable  = myri10ge_etherboot_disable;
	nic->poll     = myri10ge_etherboot_poll;
	nic->transmit = myri10ge_etherboot_transmit;
	nic->irq      = myri10ge_etherboot_irq;
	/* Record the NIC mac addr */
	memcpy (nic->node_addr, mgp->mac_addr, 6);
	/* Record driver-specific data. */
	nic->priv_data = mgp;

	printf ("%s\n", mgp->sram + ntohl (*(uint32_t*)(mgp->sram+0x3c)) + 4);
	printf ("MAC=%!\n", mgp->mac_addr);
	 
	return 1;
} 
 
static struct pci_id myri10ge_nics[] = {
	/* Each of these macros must be a single line to satisfy a script. */
	PCI_ROM(0x14c1, 0x0008, "LANai-Z8E", "Myricom LANai Z8E 10Gb Ethernet Adapter"),
}; 
 
static struct pci_driver myri10ge_driver __pci_driver = {
	.type = NIC_DRIVER,
	.name = "MYRI10GE",
	.probe = myri10ge_etherboot_probe,
	.ids = myri10ge_nics,
	.id_count = sizeof(myri10ge_nics)/sizeof(myri10ge_nics[0]),
	.class = 0,
}; 
