/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#ifndef _MISC_H_
#define _MISC_H_

struct ex_list
{
    int ex_pty;          /* Do we want a pty? */
    int ex_addr;         /* The last byte of the address */
    int ex_fport;        /* Port to telnet to */
    char *ex_exec;       /* Command line of what to exec */
    struct ex_list *ex_next;
};

extern struct ex_list *exec_list;

#define EMU_NONE      0x0

/* TCP emulations */
#define EMU_CTL       0x1
#define EMU_FTP       0x2
#define EMU_KSH       0x3
#define EMU_IRC       0x4
#define EMU_REALAUDIO 0x5
#define EMU_RLOGIN    0x6
#define EMU_IDENT     0x7
#define EMU_RSH       0x8

#define EMU_NOCONNECT 0x10      /* Don't connect */

/* UDP emulations */
#define EMU_TALK      0x1
#define EMU_NTALK     0x2
#define EMU_CUSEEME   0x3

struct tos_t
{
    u_int16_t lport;
    u_int16_t fport;
    u_int8_t tos;
    u_int8_t emu;
};

struct emu_t
{
    u_int16_t lport;
    u_int16_t fport;
    u_int8_t tos;
    u_int8_t emu;
    struct emu_t *next;
};

extern struct emu_t *tcpemu;

extern int x_port, x_server, x_display;

int show_x (char *, struct socket *);
void redir_x (u_int32_t, int, int, int);
void getouraddr (PNATState);
void slirp_insque  (PNATState, void *, void *);
void slirp_remque  (PNATState, void *);
int add_exec (struct ex_list **, int, char *, int, int);
int slirp_openpty (int *, int *);
int fork_exec (PNATState, struct socket *, char *, int);
void snooze_hup (int);
void snooze (void);
void relay (int);
void add_emu (char *);
void fd_nonblock (int);
void fd_block (int);

#endif
