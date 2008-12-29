/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

void if_encap(PNATState pData, uint16_t eth_proto, const uint8_t *ip_data, int ip_data_len);
