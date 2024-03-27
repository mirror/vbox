/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Jacob Hoffman-Andrews,
 * <github@hoffman-andrews.com>
 * Copyright (C) kpcyrd, <kpcyrd@archlinux.org>
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "curl_setup.h"

#ifdef USE_RUSTLS

#include "curl_printf.h"

#include <errno.h>
#include <rustls.h>

#include "inet_pton.h"
#include "urldata.h"
#include "sendf.h"
#include "vtls.h"
#include "vtls_int.h"
#include "select.h"
#include "strerror.h"
#include "multiif.h"
#include "connect.h" /* for the connect timeout */

struct rustls_ssl_backend_data
{
  const struct rustls_client_config *config;
  struct rustls_connection *conn;
  bool data_pending;
};

/* For a given rustls_result error code, return the best-matching CURLcode. */
static CURLcode map_error(rustls_result r)
{
  if(rustls_result_is_cert_error(r)) {
    return CURLE_PEER_FAILED_VERIFICATION;
  }
  switch(r) {
    case RUSTLS_RESULT_OK:
      return CURLE_OK;
    case RUSTLS_RESULT_NULL_PARAMETER:
      return CURLE_BAD_FUNCTION_ARGUMENT;
    default:
      return CURLE_READ_ERROR;
  }
}

static bool
cr_data_pending(struct Curl_cfilter *cf, const struct Curl_easy *data)
{
  struct ssl_connect_data *ctx = cf->ctx;
  struct rustls_ssl_backend_data *backend;

  (void)data;
  DEBUGASSERT(ctx && ctx->backend);
  backend = (struct rustls_ssl_backend_data *)ctx->backend;
  return backend->data_pending;
}

struct io_ctx {
  struct Curl_cfilter *cf;
  struct Curl_easy *data;
};

static int
read_cb(void *userdata, uint8_t *buf, uintptr_t len, uintptr_t *out_n)
{
  struct io_ctx *io_ctx = userdata;
  struct ssl_connect_data *const connssl = io_ctx->cf->ctx;
  CURLcode result;
  int ret = 0;
  ssize_t nread = Curl_conn_cf_recv(io_ctx->cf->next, io_ctx->data,
                                    (char *)buf, len, &result);
  if(nread < 0) {
    nread = 0;
    if(CURLE_AGAIN == result)
      ret = EAGAIN;
    else
      ret = EINVAL;
  }
  else if(nread == 0)
    connssl->peer_closed = TRUE;
  *out_n = (int)nread;
  return ret;
}

static int
write_cb(void *userdata, const uint8_t *buf, uintptr_t len, uintptr_t *out_n)
{
  struct io_ctx *io_ctx = userdata;
  CURLcode result;
  int ret = 0;
  ssize_t nwritten = Curl_conn_cf_send(io_ctx->cf->next, io_ctx->data,
                                       (const char *)buf, len, &result);
  if(nwritten < 0) {
    nwritten = 0;
    if(CURLE_AGAIN == result)
      ret = EAGAIN;
    else
      ret = EINVAL;
  }
  *out_n = (int)nwritten;
  /*
  CURL_TRC_CFX(io_ctx->data, io_ctx->cf, "cf->next send(len=%zu) -> %zd, %d",
                len, nwritten, result));
  */
  return ret;
}

static ssize_t tls_recv_more(struct Curl_cfilter *cf,
                             struct Curl_easy *data, CURLcode *err)
{
  struct ssl_connect_data *const connssl = cf->ctx;
  struct rustls_ssl_backend_data *const backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  struct io_ctx io_ctx;
  size_t tls_bytes_read = 0;
  rustls_io_result io_error;
  rustls_result rresult = 0;

  io_ctx.cf = cf;
  io_ctx.data = data;
  io_error = rustls_connection_read_tls(backend->conn, read_cb, &io_ctx,
                                        &tls_bytes_read);
  if(io_error == EAGAIN || io_error == EWOULDBLOCK) {
    *err = CURLE_AGAIN;
    return -1;
  }
  else if(io_error) {
    char buffer[STRERROR_LEN];
    failf(data, "reading from socket: %s",
          Curl_strerror(io_error, buffer, sizeof(buffer)));
    *err = CURLE_READ_ERROR;
    return -1;
  }

  rresult = rustls_connection_process_new_packets(backend->conn);
  if(rresult != RUSTLS_RESULT_OK) {
    char errorbuf[255];
    size_t errorlen;
    rustls_error(rresult, errorbuf, sizeof(errorbuf), &errorlen);
    failf(data, "rustls_connection_process_new_packets: %.*s",
      (int)errorlen, errorbuf);
    *err = map_error(rresult);
    return -1;
  }

  backend->data_pending = TRUE;
  *err = CURLE_OK;
  return (ssize_t)tls_bytes_read;
}

/*
 * On each run:
 *  - Read a chunk of bytes from the socket into rustls' TLS input buffer.
 *  - Tell rustls to process any new packets.
 *  - Read out as many plaintext bytes from rustls as possible, until hitting
 *    error, EOF, or EAGAIN/EWOULDBLOCK, or plainbuf/plainlen is filled up.
 *
 * It's okay to call this function with plainbuf == NULL and plainlen == 0.
 * In that case, it will copy bytes from the socket into rustls' TLS input
 * buffer, and process packets, but won't consume bytes from rustls' plaintext
 * output buffer.
 */
static ssize_t
cr_recv(struct Curl_cfilter *cf, struct Curl_easy *data,
            char *plainbuf, size_t plainlen, CURLcode *err)
{
  struct ssl_connect_data *const connssl = cf->ctx;
  struct rustls_ssl_backend_data *const backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  struct rustls_connection *rconn = NULL;
  size_t n = 0;
  size_t plain_bytes_copied = 0;
  rustls_result rresult = 0;
  ssize_t nread;
  bool eof = FALSE;

  DEBUGASSERT(backend);
  rconn = backend->conn;

  while(plain_bytes_copied < plainlen) {
    if(!backend->data_pending) {
      if(tls_recv_more(cf, data, err) < 0) {
        if(*err != CURLE_AGAIN) {
          nread = -1;
          goto out;
        }
        break;
      }
    }

    rresult = rustls_connection_read(rconn,
      (uint8_t *)plainbuf + plain_bytes_copied,
      plainlen - plain_bytes_copied,
      &n);
    if(rresult == RUSTLS_RESULT_PLAINTEXT_EMPTY) {
      backend->data_pending = FALSE;
    }
    else if(rresult == RUSTLS_RESULT_UNEXPECTED_EOF) {
      failf(data, "rustls: peer closed TCP connection "
        "without first closing TLS connection");
      *err = CURLE_READ_ERROR;
      nread = -1;
      goto out;
    }
    else if(rresult != RUSTLS_RESULT_OK) {
      /* n always equals 0 in this case, don't need to check it */
      char errorbuf[255];
      size_t errorlen;
      rustls_error(rresult, errorbuf, sizeof(errorbuf), &errorlen);
      failf(data, "rustls_connection_read: %.*s", (int)errorlen, errorbuf);
      *err = CURLE_READ_ERROR;
      nread = -1;
      goto out;
    }
    else if(n == 0) {
      /* n == 0 indicates clean EOF, but we may have read some other
         plaintext bytes before we reached this. Break out of the loop
         so we can figure out whether to return success or EOF. */
      eof = TRUE;
      break;
    }
    else {
      plain_bytes_copied += n;
    }
  }

  if(plain_bytes_copied) {
    *err = CURLE_OK;
    nread = (ssize_t)plain_bytes_copied;
  }
  else if(eof) {
    *err = CURLE_OK;
    nread = 0;
  }
  else {
    *err = CURLE_AGAIN;
    nread = -1;
  }

out:
  CURL_TRC_CF(data, cf, "cf_recv(len=%zu) -> %zd, %d",
              plainlen, nread, *err);
  return nread;
}

/*
 * On each call:
 *  - Copy `plainlen` bytes into rustls' plaintext input buffer (if > 0).
 *  - Fully drain rustls' plaintext output buffer into the socket until
 *    we get either an error or EAGAIN/EWOULDBLOCK.
 *
 * It's okay to call this function with plainbuf == NULL and plainlen == 0.
 * In that case, it won't read anything into rustls' plaintext input buffer.
 * It will only drain rustls' plaintext output buffer into the socket.
 */
static ssize_t
cr_send(struct Curl_cfilter *cf, struct Curl_easy *data,
        const void *plainbuf, size_t plainlen, CURLcode *err)
{
  struct ssl_connect_data *const connssl = cf->ctx;
  struct rustls_ssl_backend_data *const backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  struct rustls_connection *rconn = NULL;
  struct io_ctx io_ctx;
  size_t plainwritten = 0;
  size_t tlswritten = 0;
  size_t tlswritten_total = 0;
  rustls_result rresult;
  rustls_io_result io_error;
  char errorbuf[256];
  size_t errorlen;

  DEBUGASSERT(backend);
  rconn = backend->conn;

  CURL_TRC_CF(data, cf, "cf_send: %zu plain bytes", plainlen);

  io_ctx.cf = cf;
  io_ctx.data = data;

  if(plainlen > 0) {
    rresult = rustls_connection_write(rconn, plainbuf, plainlen,
                                      &plainwritten);
    if(rresult != RUSTLS_RESULT_OK) {
      rustls_error(rresult, errorbuf, sizeof(errorbuf), &errorlen);
      failf(data, "rustls_connection_write: %.*s", (int)errorlen, errorbuf);
      *err = CURLE_WRITE_ERROR;
      return -1;
    }
    else if(plainwritten == 0) {
      failf(data, "rustls_connection_write: EOF");
      *err = CURLE_WRITE_ERROR;
      return -1;
    }
  }

  while(rustls_connection_wants_write(rconn)) {
    io_error = rustls_connection_write_tls(rconn, write_cb, &io_ctx,
                                           &tlswritten);
    if(io_error == EAGAIN || io_error == EWOULDBLOCK) {
      CURL_TRC_CF(data, cf, "cf_send: EAGAIN after %zu bytes",
                  tlswritten_total);
      *err = CURLE_AGAIN;
      return -1;
    }
    else if(io_error) {
      char buffer[STRERROR_LEN];
      failf(data, "writing to socket: %s",
            Curl_strerror(io_error, buffer, sizeof(buffer)));
      *err = CURLE_WRITE_ERROR;
      return -1;
    }
    if(tlswritten == 0) {
      failf(data, "EOF in swrite");
      *err = CURLE_WRITE_ERROR;
      return -1;
    }
    CURL_TRC_CF(data, cf, "cf_send: wrote %zu TLS bytes", tlswritten);
    tlswritten_total += tlswritten;
  }

  return plainwritten;
}

/* A server certificate verify callback for rustls that always returns
   RUSTLS_RESULT_OK, or in other words disable certificate verification. */
static uint32_t
cr_verify_none(void *userdata UNUSED_PARAM,
               const rustls_verify_server_cert_params *params UNUSED_PARAM)
{
  return RUSTLS_RESULT_OK;
}

static bool
cr_hostname_is_ip(const char *hostname)
{
  struct in_addr in;
#ifdef ENABLE_IPV6
  struct in6_addr in6;
  if(Curl_inet_pton(AF_INET6, hostname, &in6) > 0) {
    return true;
  }
#endif /* ENABLE_IPV6 */
  if(Curl_inet_pton(AF_INET, hostname, &in) > 0) {
    return true;
  }
  return false;
}

static CURLcode
cr_init_backend(struct Curl_cfilter *cf, struct Curl_easy *data,
                struct rustls_ssl_backend_data *const backend)
{
  struct ssl_connect_data *connssl = cf->ctx;
  struct ssl_primary_config *conn_config = Curl_ssl_cf_get_primary_config(cf);
  struct rustls_connection *rconn = NULL;
  struct rustls_client_config_builder *config_builder = NULL;
  const struct rustls_root_cert_store *roots = NULL;
  struct rustls_root_cert_store_builder *roots_builder = NULL;
  struct rustls_web_pki_server_cert_verifier_builder *verifier_builder = NULL;
  struct rustls_server_cert_verifier *server_cert_verifier = NULL;
  const struct curl_blob *ca_info_blob = conn_config->ca_info_blob;
  const char * const ssl_cafile =
    /* CURLOPT_CAINFO_BLOB overrides CURLOPT_CAINFO */
    (ca_info_blob ? NULL : conn_config->CAfile);
  const bool verifypeer = conn_config->verifypeer;
  const char *hostname = connssl->peer.hostname;
  char errorbuf[256];
  size_t errorlen;
  int result;

  DEBUGASSERT(backend);
  rconn = backend->conn;

  config_builder = rustls_client_config_builder_new();
  if(connssl->alpn) {
    struct alpn_proto_buf proto;
    rustls_slice_bytes alpn[ALPN_ENTRIES_MAX];
    size_t i;

    for(i = 0; i < connssl->alpn->count; ++i) {
      alpn[i].data = (const uint8_t *)connssl->alpn->entries[i];
      alpn[i].len = strlen(connssl->alpn->entries[i]);
    }
    rustls_client_config_builder_set_alpn_protocols(config_builder, alpn,
                                                    connssl->alpn->count);
    Curl_alpn_to_proto_str(&proto, connssl->alpn);
    infof(data, VTLS_INFOF_ALPN_OFFER_1STR, proto.data);
  }
  if(!verifypeer) {
    rustls_client_config_builder_dangerous_set_certificate_verifier(
      config_builder, cr_verify_none);
    /* rustls doesn't support IP addresses (as of 0.19.0), and will reject
     * connections created with an IP address, even when certificate
     * verification is turned off. Set a placeholder hostname and disable
     * SNI. */
    if(cr_hostname_is_ip(hostname)) {
      rustls_client_config_builder_set_enable_sni(config_builder, false);
      hostname = "example.invalid";
    }
  }
  else if(ca_info_blob || ssl_cafile) {
    roots_builder = rustls_root_cert_store_builder_new();

    if(ca_info_blob) {
      /* Enable strict parsing only if verification isn't disabled. */
      result = rustls_root_cert_store_builder_add_pem(roots_builder,
                                                      ca_info_blob->data,
                                                      ca_info_blob->len,
                                                      verifypeer);
      if(result != RUSTLS_RESULT_OK) {
        failf(data, "rustls: failed to parse trusted certificates from blob");
        rustls_root_cert_store_builder_free(roots_builder);
        rustls_client_config_free(
          rustls_client_config_builder_build(config_builder));
        return CURLE_SSL_CACERT_BADFILE;
      }
    }
    else if(ssl_cafile) {
      /* Enable strict parsing only if verification isn't disabled. */
      result = rustls_root_cert_store_builder_load_roots_from_file(
        roots_builder, ssl_cafile, verifypeer);
      if(result != RUSTLS_RESULT_OK) {
        failf(data, "rustls: failed to load trusted certificates");
        rustls_root_cert_store_builder_free(roots_builder);
        rustls_client_config_free(
          rustls_client_config_builder_build(config_builder));
        return CURLE_SSL_CACERT_BADFILE;
      }
    }

    result = rustls_root_cert_store_builder_build(roots_builder, &roots);
    rustls_root_cert_store_builder_free(roots_builder);
    if(result != RUSTLS_RESULT_OK) {
      failf(data, "rustls: failed to load trusted certificates");
      rustls_client_config_free(
        rustls_client_config_builder_build(config_builder));
      return CURLE_SSL_CACERT_BADFILE;
    }

    verifier_builder = rustls_web_pki_server_cert_verifier_builder_new(roots);

    result = rustls_web_pki_server_cert_verifier_builder_build(
      verifier_builder, &server_cert_verifier);
    rustls_web_pki_server_cert_verifier_builder_free(verifier_builder);
    if(result != RUSTLS_RESULT_OK) {
      failf(data, "rustls: failed to load trusted certificates");
      rustls_server_cert_verifier_free(server_cert_verifier);
      rustls_client_config_free(
        rustls_client_config_builder_build(config_builder));
      return CURLE_SSL_CACERT_BADFILE;
    }

    rustls_client_config_builder_set_server_verifier(config_builder,
                                                     server_cert_verifier);
  }

  backend->config = rustls_client_config_builder_build(config_builder);
  DEBUGASSERT(rconn == NULL);
  {
    /* rustls claims to manage ip address hostnames as well here. So,
     * if we have an SNI, we use it, otherwise we pass the hostname */
    char *server = connssl->peer.sni?
                   connssl->peer.sni : connssl->peer.hostname;
    result = rustls_client_connection_new(backend->config, server, &rconn);
  }
  if(result != RUSTLS_RESULT_OK) {
    rustls_error(result, errorbuf, sizeof(errorbuf), &errorlen);
    failf(data, "rustls_client_connection_new: %.*s", (int)errorlen, errorbuf);
    return CURLE_COULDNT_CONNECT;
  }
  rustls_connection_set_userdata(rconn, backend);
  backend->conn = rconn;
  return CURLE_OK;
}

static void
cr_set_negotiated_alpn(struct Curl_cfilter *cf, struct Curl_easy *data,
  const struct rustls_connection *rconn)
{
  const uint8_t *protocol = NULL;
  size_t len = 0;

  rustls_connection_get_alpn_protocol(rconn, &protocol, &len);
  Curl_alpn_set_negotiated(cf, data, protocol, len);
}

/* Given an established network connection, do a TLS handshake.
 *
 * If `blocking` is true, this function will block until the handshake is
 * complete. Otherwise it will return as soon as I/O would block.
 *
 * For the non-blocking I/O case, this function will set `*done` to true
 * once the handshake is complete. This function never reads the value of
 * `*done*`.
 */
static CURLcode
cr_connect_common(struct Curl_cfilter *cf,
                  struct Curl_easy *data,
                  bool blocking,
                  bool *done)
{
  struct ssl_connect_data *const connssl = cf->ctx;
  curl_socket_t sockfd = Curl_conn_cf_get_socket(cf, data);
  struct rustls_ssl_backend_data *const backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  struct rustls_connection *rconn = NULL;
  CURLcode tmperr = CURLE_OK;
  int result;
  int what;
  bool wants_read;
  bool wants_write;
  curl_socket_t writefd;
  curl_socket_t readfd;
  timediff_t timeout_ms;
  timediff_t socket_check_timeout;

  DEBUGASSERT(backend);

  if(ssl_connection_none == connssl->state) {
    result = cr_init_backend(cf, data,
               (struct rustls_ssl_backend_data *)connssl->backend);
    if(result != CURLE_OK) {
      return result;
    }
    connssl->state = ssl_connection_negotiating;
  }

  rconn = backend->conn;

  /* Read/write data until the handshake is done or the socket would block. */
  for(;;) {
    /*
    * Connection has been established according to rustls. Set send/recv
    * handlers, and update the state machine.
    */
    if(!rustls_connection_is_handshaking(rconn)) {
      infof(data, "Done handshaking");
      /* Done with the handshake. Set up callbacks to send/receive data. */
      connssl->state = ssl_connection_complete;

      cr_set_negotiated_alpn(cf, data, rconn);

      *done = TRUE;
      return CURLE_OK;
    }

    wants_read = rustls_connection_wants_read(rconn);
    wants_write = rustls_connection_wants_write(rconn);
    DEBUGASSERT(wants_read || wants_write);
    writefd = wants_write?sockfd:CURL_SOCKET_BAD;
    readfd = wants_read?sockfd:CURL_SOCKET_BAD;

    /* check allowed time left */
    timeout_ms = Curl_timeleft(data, NULL, TRUE);

    if(timeout_ms < 0) {
      /* no need to continue if time already is up */
      failf(data, "rustls: operation timed out before socket check");
      return CURLE_OPERATION_TIMEDOUT;
    }

    socket_check_timeout = blocking?timeout_ms:0;

    what = Curl_socket_check(
      readfd, CURL_SOCKET_BAD, writefd, socket_check_timeout);
    if(what < 0) {
      /* fatal error */
      failf(data, "select/poll on SSL socket, errno: %d", SOCKERRNO);
      return CURLE_SSL_CONNECT_ERROR;
    }
    if(blocking && 0 == what) {
      failf(data, "rustls connection timeout after %"
        CURL_FORMAT_TIMEDIFF_T " ms", socket_check_timeout);
      return CURLE_OPERATION_TIMEDOUT;
    }
    if(0 == what) {
      infof(data, "Curl_socket_check: %s would block",
            wants_read&&wants_write ? "writing and reading" :
            wants_write ? "writing" : "reading");
      *done = FALSE;
      return CURLE_OK;
    }
    /* socket is readable or writable */

    if(wants_write) {
      infof(data, "rustls_connection wants us to write_tls.");
      cr_send(cf, data, NULL, 0, &tmperr);
      if(tmperr == CURLE_AGAIN) {
        infof(data, "writing would block");
        /* fall through */
      }
      else if(tmperr != CURLE_OK) {
        return tmperr;
      }
    }

    if(wants_read) {
      infof(data, "rustls_connection wants us to read_tls.");

      if(tls_recv_more(cf, data, &tmperr) < 0) {
        if(tmperr == CURLE_AGAIN) {
          infof(data, "reading would block");
          /* fall through */
        }
        else if(tmperr == CURLE_READ_ERROR) {
          return CURLE_SSL_CONNECT_ERROR;
        }
        else {
          return tmperr;
        }
      }
    }
  }

  /* We should never fall through the loop. We should return either because
     the handshake is done or because we can't read/write without blocking. */
  DEBUGASSERT(false);
}

static CURLcode
cr_connect_nonblocking(struct Curl_cfilter *cf,
                       struct Curl_easy *data, bool *done)
{
  return cr_connect_common(cf, data, false, done);
}

static CURLcode
cr_connect_blocking(struct Curl_cfilter *cf UNUSED_PARAM,
           struct Curl_easy *data UNUSED_PARAM)
{
  bool done; /* unused */
  return cr_connect_common(cf, data, true, &done);
}

static void cr_adjust_pollset(struct Curl_cfilter *cf,
                              struct Curl_easy *data,
                              struct easy_pollset *ps)
{
  if(!cf->connected) {
    curl_socket_t sock = Curl_conn_cf_get_socket(cf->next, data);
    struct ssl_connect_data *const connssl = cf->ctx;
    struct rustls_ssl_backend_data *const backend =
      (struct rustls_ssl_backend_data *)connssl->backend;
    struct rustls_connection *rconn = NULL;

    (void)data;
    DEBUGASSERT(backend);
    rconn = backend->conn;

    if(rustls_connection_wants_write(rconn)) {
      Curl_pollset_add_out(data, ps, sock);
    }
    if(rustls_connection_wants_read(rconn)) {
      Curl_pollset_add_in(data, ps, sock);
    }
  }
}

static void *
cr_get_internals(struct ssl_connect_data *connssl,
                 CURLINFO info UNUSED_PARAM)
{
  struct rustls_ssl_backend_data *backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  DEBUGASSERT(backend);
  return &backend->conn;
}

static void
cr_close(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct ssl_connect_data *connssl = cf->ctx;
  struct rustls_ssl_backend_data *backend =
    (struct rustls_ssl_backend_data *)connssl->backend;
  CURLcode tmperr = CURLE_OK;
  ssize_t n = 0;

  DEBUGASSERT(backend);

  if(backend->conn && !connssl->peer_closed) {
    rustls_connection_send_close_notify(backend->conn);
    n = cr_send(cf, data, NULL, 0, &tmperr);
    if(n < 0) {
      failf(data, "rustls: error sending close_notify: %d", tmperr);
    }

    rustls_connection_free(backend->conn);
    backend->conn = NULL;
  }
  if(backend->config) {
    rustls_client_config_free(backend->config);
    backend->config = NULL;
  }
}

static size_t cr_version(char *buffer, size_t size)
{
  struct rustls_str ver = rustls_version();
  return msnprintf(buffer, size, "%.*s", (int)ver.len, ver.data);
}

const struct Curl_ssl Curl_ssl_rustls = {
  { CURLSSLBACKEND_RUSTLS, "rustls" },
  SSLSUPP_CAINFO_BLOB |            /* supports */
  SSLSUPP_TLS13_CIPHERSUITES |
  SSLSUPP_HTTPS_PROXY,
  sizeof(struct rustls_ssl_backend_data),

  Curl_none_init,                  /* init */
  Curl_none_cleanup,               /* cleanup */
  cr_version,                      /* version */
  Curl_none_check_cxn,             /* check_cxn */
  Curl_none_shutdown,              /* shutdown */
  cr_data_pending,                 /* data_pending */
  Curl_none_random,                /* random */
  Curl_none_cert_status_request,   /* cert_status_request */
  cr_connect_blocking,             /* connect */
  cr_connect_nonblocking,          /* connect_nonblocking */
  cr_adjust_pollset,               /* adjust_pollset */
  cr_get_internals,                /* get_internals */
  cr_close,                        /* close_one */
  Curl_none_close_all,             /* close_all */
  Curl_none_session_free,          /* session_free */
  Curl_none_set_engine,            /* set_engine */
  Curl_none_set_engine_default,    /* set_engine_default */
  Curl_none_engines_list,          /* engines_list */
  Curl_none_false_start,           /* false_start */
  NULL,                            /* sha256sum */
  NULL,                            /* associate_connection */
  NULL,                            /* disassociate_connection */
  NULL,                            /* free_multi_ssl_backend_data */
  cr_recv,                         /* recv decrypted data */
  cr_send,                         /* send data to encrypt */
};

#endif /* USE_RUSTLS */
