/*
 * tftp.c - a simple, read-only tftp server for qemu
 *
 * Copyright (c) 2004 Magnus Damm <damm@opensource.se>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <slirp.h>

#ifndef VBOX
struct tftp_session {
    int in_use;
    unsigned char filename[TFTP_FILENAME_MAX];

    struct in_addr client_ip;
    u_int16_t client_port;

    int timestamp;
};

struct tftp_session tftp_sessions[TFTP_SESSIONS_MAX];

const char *tftp_prefix;
#endif /* !VBOX */

#ifdef VBOX
static void tftp_session_update(PNATState pData, struct tftp_session *spt)
#else /* !VBOX */
static void tftp_session_update(struct tftp_session *spt)
#endif /* !VBOX */
{
    spt->timestamp = curtime;
    spt->in_use = 1;
}

static void tftp_session_terminate(struct tftp_session *spt)
{
  spt->in_use = 0;
}

#ifdef VBOX
static int tftp_session_allocate(PNATState pData, struct tftp_t *tp)
#else /* !VBOX */
static int tftp_session_allocate(struct tftp_t *tp)
#endif /* !VBOX */
{
  struct tftp_session *spt;
  int k;

  for (k = 0; k < TFTP_SESSIONS_MAX; k++) {
    spt = &tftp_sessions[k];

    if (!spt->in_use)
        goto found;

    /* sessions time out after 5 inactive seconds */
    if ((int)(curtime - spt->timestamp) > 5000)
        goto found;
  }

  return -1;

 found:
  memset(spt, 0, sizeof(*spt));
  memcpy(&spt->client_ip, &tp->ip.ip_src, sizeof(spt->client_ip));
  spt->client_port = tp->udp.uh_sport;

#ifdef VBOX
  tftp_session_update(pData, spt);
#else /* !VBOX */
  tftp_session_update(spt);
#endif /* !VBOX */

  return k;
}

#ifdef VBOX
static int tftp_session_find(PNATState pData, struct tftp_t *tp)
#else /* !VBOX */
static int tftp_session_find(struct tftp_t *tp)
#endif /* !VBOX */
{
  struct tftp_session *spt;
  int k;

  for (k = 0; k < TFTP_SESSIONS_MAX; k++) {
    spt = &tftp_sessions[k];

    if (spt->in_use) {
      if (!memcmp(&spt->client_ip, &tp->ip.ip_src, sizeof(spt->client_ip))) {
	if (spt->client_port == tp->udp.uh_sport) {
	  return k;
	}
      }
    }
  }

  return -1;
}

#ifdef VBOX
static int tftp_read_data(PNATState pData, struct tftp_session *spt, u_int16_t block_nr,
			  u_int8_t *buf, int len)
#else /* !VBOX */
static int tftp_read_data(struct tftp_session *spt, u_int16_t block_nr,
			  u_int8_t *buf, int len)
#endif /* !VBOX */
{
  int fd;
  int bytes_read = 0;
  char buffer[1024];
  int n;

#ifndef VBOX
  n = snprintf(buffer, sizeof(buffer), "%s/%s",
	       tftp_prefix, spt->filename);
#else
  n = RTStrPrintf(buffer, sizeof(buffer), "%s/%s",
	       tftp_prefix, spt->filename);
#endif
  if (n >= sizeof(buffer))
    return -1;

  fd = open(buffer, O_RDONLY | O_BINARY);

  if (fd < 0) {
    return -1;
  }

  if (len) {
    lseek(fd, block_nr * 512, SEEK_SET);

    bytes_read = read(fd, buf, len);
  }

  close(fd);

  return bytes_read;
}

#ifdef VBOX
static int tftp_send_oack(PNATState pData,
                          struct tftp_session *spt,
                          const char *key, uint32_t value,
                          struct tftp_t *recv_tp)
#else /* !VBOX */
static int tftp_send_oack(struct tftp_session *spt,
                          const char *key, uint32_t value,
                          struct tftp_t *recv_tp)
#endif /* !VBOX */
{
    struct sockaddr_in saddr, daddr;
    struct mbuf *m;
    struct tftp_t *tp;
    int n = 0;

#ifdef VBOX
    m = m_get(pData);
#else /* !VBOX */
    m = m_get();
#endif /* !VBOX */

    if (!m)
	return -1;

    memset(m->m_data, 0, m->m_size);

    m->m_data += if_maxlinkhdr;
    tp = (void *)m->m_data;
    m->m_data += sizeof(struct udpiphdr);

    tp->tp_op = htons(TFTP_OACK);
#ifdef VBOX
    n += sprintf((char *)tp->x.tp_buf + n, "%s", key) + 1;
    n += sprintf((char *)tp->x.tp_buf + n, "%u", value) + 1;
#else /* !VBOX */
    n += sprintf(tp->x.tp_buf + n, "%s", key) + 1;
    n += sprintf(tp->x.tp_buf + n, "%u", value) + 1;
#endif /* !VBOX */

    saddr.sin_addr = recv_tp->ip.ip_dst;
    saddr.sin_port = recv_tp->udp.uh_dport;

    daddr.sin_addr = spt->client_ip;
    daddr.sin_port = spt->client_port;

    m->m_len = sizeof(struct tftp_t) - 514 + n -
        sizeof(struct ip) - sizeof(struct udphdr);
#ifdef VBOX
    udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#else /* !VBOX */
    udp_output2(NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#endif /* !VBOX */

    return 0;
}



#ifdef VBOX
static int tftp_send_error(PNATState pData,
                           struct tftp_session *spt,
			   u_int16_t errorcode, const char *msg,
			   struct tftp_t *recv_tp)
#else /* !VBOX */
static int tftp_send_error(struct tftp_session *spt,
			   u_int16_t errorcode, const char *msg,
			   struct tftp_t *recv_tp)
#endif /* !VBOX */
{
  struct sockaddr_in saddr, daddr;
  struct mbuf *m;
  struct tftp_t *tp;
  int nobytes;

#ifdef VBOX
  m = m_get(pData);
#else /* !VBOX */
  m = m_get();
#endif /* !VBOX */

  if (!m) {
    return -1;
  }

  memset(m->m_data, 0, m->m_size);

  m->m_data += if_maxlinkhdr;
  tp = (void *)m->m_data;
  m->m_data += sizeof(struct udpiphdr);

  tp->tp_op = htons(TFTP_ERROR);
  tp->x.tp_error.tp_error_code = htons(errorcode);
#ifndef VBOX
  strcpy(tp->x.tp_error.tp_msg, msg);
#else /* VBOX */
  strcpy((char *)tp->x.tp_error.tp_msg, msg);
#endif /* VBOX */

  saddr.sin_addr = recv_tp->ip.ip_dst;
  saddr.sin_port = recv_tp->udp.uh_dport;

  daddr.sin_addr = spt->client_ip;
  daddr.sin_port = spt->client_port;

  nobytes = 2;

  m->m_len = sizeof(struct tftp_t) - 514 + 3 + strlen(msg) -
        sizeof(struct ip) - sizeof(struct udphdr);

#ifdef VBOX
  udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#else /* !VBOX */
  udp_output2(NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#endif /* !VBOX */

  tftp_session_terminate(spt);

  return 0;
}

#ifdef VBOX
static int tftp_send_data(PNATState pData,
                          struct tftp_session *spt,
			  u_int16_t block_nr,
			  struct tftp_t *recv_tp)
#else /* !VBOX */
static int tftp_send_data(struct tftp_session *spt,
			  u_int16_t block_nr,
			  struct tftp_t *recv_tp)
#endif /* !VBOX */
{
  struct sockaddr_in saddr, daddr;
  struct mbuf *m;
  struct tftp_t *tp;
  int nobytes;

  if (block_nr < 1) {
    return -1;
  }

#ifdef VBOX
  m = m_get(pData);
#else /* !VBOX */
  m = m_get();
#endif /* !VBOX */

  if (!m) {
    return -1;
  }

  memset(m->m_data, 0, m->m_size);

  m->m_data += if_maxlinkhdr;
  tp = (void *)m->m_data;
  m->m_data += sizeof(struct udpiphdr);

  tp->tp_op = htons(TFTP_DATA);
  tp->x.tp_data.tp_block_nr = htons(block_nr);

  saddr.sin_addr = recv_tp->ip.ip_dst;
  saddr.sin_port = recv_tp->udp.uh_dport;

  daddr.sin_addr = spt->client_ip;
  daddr.sin_port = spt->client_port;

#ifdef VBOX
  nobytes = tftp_read_data(pData, spt, block_nr - 1, tp->x.tp_data.tp_buf, 512);
#else /* !VBOX */
  nobytes = tftp_read_data(spt, block_nr - 1, tp->x.tp_data.tp_buf, 512);
#endif /* !VBOX */

  if (nobytes < 0) {
#ifdef VBOX
    m_free(pData, m);
#else /* !VBOX */
    m_free(m);
#endif /* !VBOX */

    /* send "file not found" error back */

#ifdef VBOX
    tftp_send_error(pData, spt, 1, "File not found", tp);
#else /* !VBOX */
    tftp_send_error(spt, 1, "File not found", tp);
#endif /* !VBOX */

    return -1;
  }

  m->m_len = sizeof(struct tftp_t) - (512 - nobytes) -
        sizeof(struct ip) - sizeof(struct udphdr);

#ifdef VBOX
  udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#else /* !VBOX */
  udp_output2(NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
#endif /* !VBOX */

  if (nobytes == 512) {
#ifdef VBOX
    tftp_session_update(pData, spt);
#else /* !VBOX */
    tftp_session_update(spt);
#endif /* !VBOX */
  }
  else {
    tftp_session_terminate(spt);
  }

  return 0;
}

#ifdef VBOX
static void tftp_handle_rrq(PNATState pData, struct tftp_t *tp, int pktlen)
#else /* !VBOX */
static void tftp_handle_rrq(struct tftp_t *tp, int pktlen)
#endif /* !VBOX */
{
  struct tftp_session *spt;
  int s, k, n;
  u_int8_t *src, *dst;

#ifdef VBOX
  s = tftp_session_allocate(pData, tp);
#else /* !VBOX */
  s = tftp_session_allocate(tp);
#endif /* !VBOX */

  if (s < 0) {
    return;
  }

  spt = &tftp_sessions[s];

  src = tp->x.tp_buf;
  dst = spt->filename;
  n = pktlen - ((uint8_t *)&tp->x.tp_buf[0] - (uint8_t *)tp);

  /* get name */

  for (k = 0; k < n; k++) {
    if (k < TFTP_FILENAME_MAX) {
      dst[k] = src[k];
    }
    else {
      return;
    }

    if (src[k] == '\0') {
      break;
    }
  }

  if (k >= n) {
    return;
  }

  k++;

  /* check mode */
  if ((n - k) < 6) {
    return;
  }

  if (memcmp(&src[k], "octet\0", 6) != 0) {
#ifdef VBOX
      tftp_send_error(pData, spt, 4, "Unsupported transfer mode", tp);
#else /* !VBOX */
      tftp_send_error(spt, 4, "Unsupported transfer mode", tp);
#endif /* !VBOX */
      return;
  }

  k += 6; /* skipping octet */

  /* do sanity checks on the filename */

#ifndef VBOX
  if ((spt->filename[0] != '/')
      || (spt->filename[strlen(spt->filename) - 1] == '/')
      ||  strstr(spt->filename, "/../")) {
      tftp_send_error(spt, 2, "Access violation", tp);
#else /* VBOX */
  if ((spt->filename[0] != '/')
      || (spt->filename[strlen((const char *)spt->filename) - 1] == '/')
      ||  strstr((char *)spt->filename, "/../")) {
      tftp_send_error(pData, spt, 2, "Access violation", tp);
#endif /* VBOX */
      return;
  }

  /* only allow exported prefixes */

  if (!tftp_prefix) {
#ifdef VBOX
      tftp_send_error(pData, spt, 2, "Access violation", tp);
#else /* !VBOX */
      tftp_send_error(spt, 2, "Access violation", tp);
#endif /* !VBOX */
      return;
  }

  /* check if the file exists */

#ifdef VBOX
  if (tftp_read_data(pData, spt, 0, spt->filename, 0) < 0) {
      tftp_send_error(pData, spt, 1, "File not found", tp);
#else /* !VBOX */
  if (tftp_read_data(spt, 0, spt->filename, 0) < 0) {
      tftp_send_error(spt, 1, "File not found", tp);
#endif /* !VBOX */
      return;
  }

  if (src[n - 1] != 0) {
#ifdef VBOX
      tftp_send_error(pData, spt, 2, "Access violation", tp);
#else /* !VBOX */
      tftp_send_error(spt, 2, "Access violation", tp);
#endif /* !VBOX */
      return;
  }

  while (k < n) {
      const char *key, *value;

#ifdef VBOX
      key = (const char *)src + k;
#else /* !VBOX */
      key = src + k;
#endif /* !VBOX */
      k += strlen(key) + 1;

      if (k >= n) {
#ifdef VBOX
	  tftp_send_error(pData, spt, 2, "Access violation", tp);
#else /* !VBOX */
	  tftp_send_error(spt, 2, "Access violation", tp);
#endif /* !VBOX */
	  return;
      }

#ifdef VBOX
      value = (const char *)src + k;
#else /* !VBOX */
      value = src + k;
#endif /* !VBOX */
      k += strlen(value) + 1;

      if (strcmp(key, "tsize") == 0) {
	  int tsize = atoi(value);
	  struct stat stat_p;

	  if (tsize == 0 && tftp_prefix) {
	      char buffer[1024];
	      int len;

#ifndef VBOX
	      len = snprintf(buffer, sizeof(buffer), "%s/%s",
			     tftp_prefix, spt->filename);
#else
	      len = RTStrPrintf(buffer, sizeof(buffer), "%s/%s",
			     tftp_prefix, spt->filename);
#endif

	      if (stat(buffer, &stat_p) == 0)
		  tsize = stat_p.st_size;
	      else {
#ifdef VBOX
		  tftp_send_error(pData, spt, 1, "File not found", tp);
#else /* !VBOX */
		  tftp_send_error(spt, 1, "File not found", tp);
#endif /* !VBOX */
		  return;
	      }
	  }

#ifdef VBOX
	  tftp_send_oack(pData, spt, "tsize", tsize, tp);
#else /* !VBOX */
	  tftp_send_oack(spt, "tsize", tsize, tp);
#endif /* !VBOX */
      }
  }

#ifdef VBOX
  tftp_send_data(pData, spt, 1, tp);
#else /* !VBOX */
  tftp_send_data(spt, 1, tp);
#endif /* !VBOX */
}

#ifdef VBOX
static void tftp_handle_ack(PNATState pData, struct tftp_t *tp, int pktlen)
#else /* !VBOX */
static void tftp_handle_ack(struct tftp_t *tp, int pktlen)
#endif /* !VBOX */
{
  int s;

#ifdef VBOX
  s = tftp_session_find(pData, tp);
#else /* !VBOX */
  s = tftp_session_find(tp);
#endif /* !VBOX */

  if (s < 0) {
    return;
  }

#ifdef VBOX
  if (tftp_send_data(pData, &tftp_sessions[s],
		     ntohs(tp->x.tp_data.tp_block_nr) + 1,
		     tp) < 0) {
#else /* !VBOX */
  if (tftp_send_data(&tftp_sessions[s],
		     ntohs(tp->x.tp_data.tp_block_nr) + 1,
		     tp) < 0) {
#endif /* !VBOX */
    return;
  }
}

#ifdef VBOX
void tftp_input(PNATState pData, struct mbuf *m)
#else /* !VBOX */
void tftp_input(struct mbuf *m)
#endif /* !VBOX */
{
  struct tftp_t *tp = (struct tftp_t *)m->m_data;

  switch(ntohs(tp->tp_op)) {
  case TFTP_RRQ:
#ifdef VBOX
    tftp_handle_rrq(pData, tp, m->m_len);
#else /* !VBOX */
    tftp_handle_rrq(tp, m->m_len);
#endif /* !VBOX */
    break;

  case TFTP_ACK:
#ifdef VBOX
    tftp_handle_ack(pData, tp, m->m_len);
#else /* !VBOX */
    tftp_handle_ack(tp, m->m_len);
#endif /* !VBOX */
    break;
  }
}
