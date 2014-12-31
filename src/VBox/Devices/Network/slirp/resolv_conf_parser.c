/* $Id$ */
/** @file
 * resolv_conf_parser.c - parser of resolv.conf resolver(5)
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/net.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>

#include <arpa/inet.h>

#include <ctype.h>

#include "resolv_conf_parser.h"

/* XXX: it's required to add the aliases for keywords and
 * types to handle conditions more clearly */
enum RCP_TOKEN
{
  tok_eof = -1, /* EOF */
  tok_string = -2, /* string */
  tok_number = -3, /* number */
  tok_ipv4 = -4, /* ipv4 */
  tok_ipv4_port = -5, /* ipv4 port */
  tok_ipv6 = -6, /* ipv6 */
  tok_ipv6_port = -7, /* ipv6 port */
  /* keywords */
  tok_nameserver = -8, /* nameserver */
  tok_port = -9, /* port, Mac OSX specific */
  tok_domain = -10, /* domain */
  tok_search = -11, /* search */
  tok_search_order = -12, /* search order */
  tok_sortlist = -13, /* sortlist */
  tok_timeout = -14, /* timeout */
  tok_options = -15, /* options */
  tok_option = -16, /* option */
  tok_comment = -17, /* comment */
  tok_error = -20
};

#define RCP_BUFFER_SIZE 256


struct rcp_parser
{
    enum RCP_TOKEN rcpp_token;
    char rcpp_str_buffer[RCP_BUFFER_SIZE];
    struct rcp_state *rcpp_state;
    PRTSTREAM rcpp_stream;
};


#define GETCHAR(parser) (RTStrmGetCh((parser)->rcpp_stream))
#define EOF (-1)


#define PARSER_STOP(tok, parser, ptr) (   (tok) != EOF \
                                       && (((ptr) - (parser)->rcpp_str_buffer) != (RCP_BUFFER_SIZE - 1)))
#define PARSER_BUFFER_EXCEEDED(parser, ptr)                             \
  do {                                                                  \
      if (((ptr) - (parser)->rcpp_str_buffer) == (RCP_BUFFER_SIZE - 1)) { \
          return tok_error;                                             \
      }                                                                 \
  }while(0);

static int rcp_get_token(struct rcp_parser *parser)
{
    char tok = ' ';
    char *ptr;
    size_t ptr_len;

    while (isspace(tok))
        tok = GETCHAR(parser);

    ptr = parser->rcpp_str_buffer;

    /* tok can't be ipv4 */
    if (isalnum(tok)) {
        int xdigit, digit, dot_number;
        RT_ZERO(parser->rcpp_str_buffer);

        dot_number = 0;
        xdigit = 1;
        digit = 1;
        do {
            *ptr++ = tok;
            tok = GETCHAR(parser);

            if (!isalnum(tok) && tok != ':' && tok != '.' && tok != '-' && tok != '_')
                break;

            /**
             * if before ':' there were only [0-9][a-f][A-F],
             * then it can't be option.
             */
            xdigit &= (isxdigit(tok) || (tok == ':'));
            /**
             * We want hint to differ ipv4 and network name.
             */
            digit &= (isdigit(tok) || (tok == '.'));

            if (tok == ':')
            {
                if (xdigit == 1)
                {
                    int port = 0;
                    do
                    {
                        *ptr++ = tok;
                        tok = GETCHAR(parser);

                        if (tok == '.')
                            port++;

                    } while(PARSER_STOP(tok, parser, ptr) && (tok == ':' || tok == '.' || isxdigit(tok)));

                    PARSER_BUFFER_EXCEEDED(parser, ptr);

                    if (port == 0)
                        return tok_ipv6;
                    else if (port == 1)
                        return tok_ipv6_port;
                    else
                    {
                        /* eats rest of the token */
                        do
                        {
                            *ptr++ = tok;
                            tok = GETCHAR(parser);
                        } while(   PARSER_STOP(tok, parser, ptr)
                                && (isalnum(tok) || tok == '.'  || tok == '_' || tok == '-'));

                        PARSER_BUFFER_EXCEEDED(parser, ptr);

                        return tok_string;
                    }
                }
                else {
                    /* XXX: need further experiments */
                    return tok_option; /* option with value */
                }
            }

            if (tok == '.')
            {
                do {
                    if (tok == '.') dot_number++;

                    *ptr++ = tok;
                    digit &= (isdigit(tok) || (tok == '.'));
                    tok = GETCHAR(parser);
                } while(   PARSER_STOP(tok, parser, ptr)
                        && (isalnum(tok) || tok == '.' || tok == '_' || tok == '-'));

                PARSER_BUFFER_EXCEEDED(parser, ptr);

                if (dot_number == 3 && digit)
                    return tok_ipv4;
                else if (dot_number == 4 && digit)
                    return tok_ipv4_port;
                else
                    return tok_string;
            }
        } while(   PARSER_STOP(tok, parser, ptr)
                && (isalnum(tok) || tok == ':' || tok == '.' || tok == '-' || tok == '_'));

        PARSER_BUFFER_EXCEEDED(parser, ptr);

        if (digit || xdigit)
            return tok_number;
        if (RTStrCmp(parser->rcpp_str_buffer, "nameserver") == 0)
            return tok_nameserver;
        if (RTStrCmp(parser->rcpp_str_buffer, "port") == 0)
            return tok_port;
        if (RTStrCmp(parser->rcpp_str_buffer, "domain") == 0)
            return tok_domain;
        if (RTStrCmp(parser->rcpp_str_buffer, "search") == 0)
            return tok_search;
        if (RTStrCmp(parser->rcpp_str_buffer, "search_order") == 0)
            return tok_search_order;
        if (RTStrCmp(parser->rcpp_str_buffer, "sortlist") == 0)
            return tok_sortlist;
        if (RTStrCmp(parser->rcpp_str_buffer, "timeout") == 0)
            return tok_timeout;
        if (RTStrCmp(parser->rcpp_str_buffer, "options") == 0)
            return tok_options;

        return tok_string;
    }

    if (tok == EOF) return tok_eof;

    if (tok == '#')
    {
        do{
            tok = GETCHAR(parser);
        } while (tok != EOF && tok != '\r' && tok != '\n');

        if (tok == EOF) return tok_eof;

        return tok_comment;
    }
    return tok;
}

#undef PARSER_STOP
#undef PARSER_BUFFER_EXCEEDED

/**
 * nameserverexpr ::= 'nameserver' ip+
 * @note: resolver(5) ip ::= (ipv4|ipv6)(.number)?
 */
static enum RCP_TOKEN rcp_parse_nameserver(struct rcp_parser *parser)
{
    enum RCP_TOKEN tok = rcp_get_token(parser); /* eats 'nameserver' */

    if (  (   tok != tok_ipv4
           && tok != tok_ipv4_port
           && tok != tok_ipv6
           && tok != tok_ipv6_port)
        || tok == EOF)
        return tok_error;

    while (   tok == tok_ipv4
           || tok == tok_ipv4_port
           || tok == tok_ipv6
           || tok == tok_ipv6_port)
    {
        struct rcp_state *st;
        RTNETADDR *address;
        char *str_address;

        Assert(parser->rcpp_state);

        st = parser->rcpp_state;

        /* It's still valid resolv.conf file, just rest of the nameservers should be ignored */
        if (st->rcps_num_nameserver >= RCPS_MAX_NAMESERVERS)
            return rcp_get_token(parser);

        address = &st->rcps_nameserver[st->rcps_num_nameserver];
        str_address = &st->rcps_nameserver_str_buffer[st->rcps_num_nameserver * RCPS_IPVX_SIZE];
#ifdef RT_OS_DARWIN
        if (   tok == tok_ipv4_port
            || (   tok == tok_ipv6_port
                && (st->rcps_flags & RCPSF_IGNORE_IPV6) == 0))
        {
            char *ptr = &parser->rcpp_str_buffer[strlen(parser->rcpp_str_buffer)];
            while (*(--ptr) != '.');
            *ptr = '\0';
            address->uPort = RTStrToUInt16(ptr + 1);

            if (address->uPort == 0) return tok_error;
        }
#endif
        /**
         * if we on Darwin upper code will cut off port if it's.
         */
        if ((st->rcps_flags & RCPSF_NO_STR2IPCONV) != 0)
        {
            if (strlen(parser->rcpp_str_buffer) > RCPS_IPVX_SIZE)
                return tok_error;

            strcpy(str_address, parser->rcpp_str_buffer);

            st->rcps_str_nameserver[st->rcps_num_nameserver] = str_address;

            goto loop_prolog;
        }

        switch (tok)
        {
            case tok_ipv4:
            case tok_ipv4_port:
                {
                    int rc = RTNetStrToIPv4Addr(parser->rcpp_str_buffer, &address->uAddr.IPv4);
                    if (RT_FAILURE(rc)) return tok_error;

                    address->enmType = RTNETADDRTYPE_IPV4;
                }

                break;
            case tok_ipv6:
            case tok_ipv6_port:
                {
                    int rc;

                    if ((st->rcps_flags & RCPSF_IGNORE_IPV6) != 0)
                        return rcp_get_token(parser);

                    rc = inet_pton(AF_INET6, parser->rcpp_str_buffer,
                                       &address->uAddr.IPv6);
                    if (rc == -1)
                        return tok_error;

                    address->enmType = RTNETADDRTYPE_IPV6;
                }

                break;
            default: /* loop condition doesn't let enter enything */
                AssertMsgFailed(("shouldn't ever happen tok:%d, %s", tok,
                                 isprint(tok) ? parser->rcpp_str_buffer : "#"));
                break;
        }

    loop_prolog:
        st->rcps_num_nameserver++;
        tok = rcp_get_token(parser);
    }
    return tok;
}

/**
 * portexpr ::= 'port' [0-9]+
 */
static enum RCP_TOKEN rcp_parse_port(struct rcp_parser *parser)
{
    struct rcp_state *st;
    enum RCP_TOKEN tok = rcp_get_token(parser); /* eats 'port' */

    Assert(parser->rcpp_state);
    st = parser->rcpp_state;

    if (   tok != tok_number
        || tok == tok_eof)
        return tok_error;

    st->rcps_port = RTStrToUInt16(parser->rcpp_str_buffer);

    if (st->rcps_port == 0)
        return tok_error;

    return rcp_get_token(parser);
}

/**
 * domainexpr ::= 'domain' string
 */
static enum RCP_TOKEN rcp_parse_domain(struct rcp_parser *parser)
{
    struct rcp_state *st;
    enum RCP_TOKEN tok = rcp_get_token(parser); /* eats 'domain' */

    Assert(parser->rcpp_state);
    st = parser->rcpp_state;

    /**
     * It's nowhere specified how resolver should react on dublicats
     * of 'domain' declarations, let's assume that resolv.conf is broken.
     */
    if (   tok == tok_eof
        || tok == tok_error
        || st->rcps_domain != NULL)
        return tok_error;

    strcpy(st->rcps_domain_buffer, parser->rcpp_str_buffer);
    /**
     * We initialize this pointer in place, just make single pointer check
     * in 'domain'-less resolv.conf.
     */
    st->rcps_domain = st->rcps_domain_buffer;

    return rcp_get_token(parser);
}

/**
 * searchexpr ::= 'search' (string)+
 * @note: resolver (5) Mac OSX:
 * "The search list is currently limited to six domains with a total of 256 characters."
 * @note: resolv.conf (5) Linux:
 * "The search list is currently limited to six domains with a total of 256 characters."
 * @note: 'search' parameter could contains numbers only hex or decimal, 1c1e or 111
 */
static enum RCP_TOKEN rcp_parse_search(struct rcp_parser *parser)
{
    unsigned i, len, trailing;
    char *ptr;
    struct rcp_state *st;
    enum RCP_TOKEN tok = rcp_get_token(parser); /* eats 'search' */

    Assert(parser->rcpp_state);
    st = parser->rcpp_state;

    if (   tok == tok_eof
        || tok == tok_error)
        return tok_error;

    /* just ignore "too many search list" */
    if (st->rcps_num_searchlist >= RCPS_MAX_SEARCHLIST)
        return rcp_get_token(parser);

    /* we don't want accept keywords */
    if (tok <= tok_nameserver)
        return tok;

    /* if there're several entries of "search" we compose them together */
    i = st->rcps_num_searchlist;
    if ( i == 0)
        trailing = RCPS_BUFFER_SIZE;
    else
    {
        ptr = st->rcps_searchlist[i - 1];
        trailing = RCPS_BUFFER_SIZE - (ptr -
                                       st->rcps_searchlist_buffer + strlen(ptr) + 1);
    }

    while (1)
    {
        len = strlen(parser->rcpp_str_buffer);

        if (len + 1 > trailing)
            break; /* not enough room for new entry */

        if (i >= RCPS_MAX_SEARCHLIST)
            break; /* not enought free entries for 'search' items */

        ptr = st->rcps_searchlist_buffer + RCPS_BUFFER_SIZE - trailing;
        strcpy(ptr, parser->rcpp_str_buffer);

        trailing -= len + 1; /* 1 reserved for '\0' */

        st->rcps_searchlist[i++] = ptr;
        tok = rcp_get_token(parser);

        /* token filter */
        if (   tok == tok_eof
            || tok == tok_error
            || tok <= tok_nameserver)
            break;
    }

    st->rcps_num_searchlist = i;

    return tok;
}

/**
 * expr ::= nameserverexpr | expr
 *      ::= portexpr | expr
 *      ::= domainexpr | expr
 *      ::= searchexpr | expr
 *      ::= searchlistexpr | expr
 *      ::= search_orderexpr | expr
 *      ::= timeoutexpr | expr
 *      ::= optionsexpr | expr
 */
static int rcp_parse_primary(struct rcp_parser *parser)
{
    enum RCP_TOKEN tok;
    tok = rcp_get_token(parser);

    while(   tok != tok_eof
          && tok != tok_error)
    {
        switch (tok)
        {
            case tok_nameserver:
                tok = rcp_parse_nameserver(parser);
                break;
            case tok_port:
                tok = rcp_parse_port(parser);
                break;
            case tok_domain:
                tok = rcp_parse_domain(parser);
                break;
            case tok_search:
                tok = rcp_parse_search(parser);
                break;
            default:
                tok = rcp_get_token(parser);
        }
    }

    if (tok == tok_error)
        return -1;

    return 0;
}


int rcp_parse(struct rcp_state* state, const char *filename)
{
    unsigned i;
    uint32_t flags;
    int rc;
    struct rcp_parser parser;
    flags = state->rcps_flags;

    RT_ZERO(parser);
    RT_ZERO(*state);

    state->rcps_flags = flags;

    parser.rcpp_state = state;

    /**
     * for debugging need: with RCP_STANDALONE it's possible
     * to run simplefied scenarious like
     *
     * # cat /etc/resolv.conf | rcp-test-0
     * or in lldb
     * # process launch -i /etc/resolv.conf
     */
#ifdef RCP_STANDALONE
    if (filename == NULL)
        parser.rcpp_stream = g_pStdIn;
#else
    if (filename == NULL)
        return -1;
#endif
    else
    {
        rc = RTStrmOpen(filename, "r", &parser.rcpp_stream);
        if (RT_FAILURE(rc)) return -1;
    }

    rc = rcp_parse_primary(&parser);

    if (filename != NULL)
        RTStrmClose(parser.rcpp_stream);

    if (rc == -1)
        return -1;

#ifdef RT_OS_DARWIN
    /**
     * port recolv.conf's option and IP.port are Mac OSX extentions, there're no need to care on
     * other hosts.
     */
    if (state->rcps_port == 0)
        state->rcps_port = 53;

    for(i = 0;  (state->rcps_flags & RCPSF_NO_STR2IPCONV) == 0
             && i != RCPS_MAX_NAMESERVERS; ++i)
    {
        RTNETADDR *addr = &state->rcps_nameserver[i];

        if (addr->uPort == 0)
            addr->uPort = state->rcps_port;
    }
#endif

    if (   state->rcps_domain == NULL
        && state->rcps_searchlist[0] != NULL)
        state->rcps_domain = state->rcps_searchlist[0];

    return 0;
}
