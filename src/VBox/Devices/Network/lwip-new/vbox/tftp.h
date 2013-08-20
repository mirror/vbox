/* -*- indent-tabs-mode: nil; -*- */
#ifndef _TFTP_H_
#define _TFTP_H_

#define TFTP_SERVER_PORT 69

/* opcodes */
#define TFTP_RRQ    1
#define TFTP_WRQ    2
#define TFTP_DATA   3
#define TFTP_ACK    4
#define TFTP_ERROR  5
/* RFC 2347 */
#define TFTP_OACK   6


/* error codes */
#define TFTP_EUNDEF     0 /* Not defined, see error message (if any). */
#define TFTP_ENOENT     1 /* File not found. */
#define TFTP_EACCESS    2 /* Access violation. */
#define TFTP_EFBIG      3 /* Disk full or allocation exceeded. */
#define TFTP_ENOSYS     4 /* Illegal TFTP operation. */
#define TFTP_ESRCH      5 /* Unknown transfer ID. */
#define TFTP_EEXIST     6 /* File already exists. */
#define TFTP_EUSER      7 /* No such user. */
/* RFC 2347 */
#define TFTP_EONAK      8 /* Option refused. */


#endif  /* _TFTP_H_ */
