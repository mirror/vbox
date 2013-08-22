/* -*- indent-tabs-mode: nil; -*- */
#ifndef _pxtcp_h_
#define _pxtcp_h_

struct pxtcp;
struct fwspec;

struct pxtcp *pxtcp_create_forwarded(SOCKET);
void pxtcp_cancel_forwarded(struct pxtcp *);

void pxtcp_pcb_connect(struct pxtcp *, const struct fwspec *);

int pxtcp_pmgr_add(struct pxtcp *);
void pxtcp_pmgr_del(struct pxtcp *);

#endif  /* _pxtcp_h_ */
