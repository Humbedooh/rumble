/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "comm.h"
#ifndef RUMBLE_WINSOCK
#   include <arpa/nameser.h>
#   include <resolv.h>
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dvector *comm_mxLookup(const char *domain) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    dvector     *vec = dvector_init();
#ifdef RUMBLE_WINSOCK   /* Windows MX resolver */
    DNS_STATUS  status;
    PDNS_RECORD rec,
                prec;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    status = DnsQuery_A(domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, 0, &rec, 0);
    prec = rec;
    if (!status) {
        while (rec) {
            if (rec->wType == DNS_TYPE_MX) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                size_t      len;
                mxRecord    *mx = (mxRecord *) malloc(sizeof(mxRecord));
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                if (!mx) merror();
                len = strlen((char *) rec->Data.MX.pNameExchange);
                mx->host = (char *) calloc(1, len + 1);
                if (!mx->host) merror();
                strncpy((char *) mx->host, (char *) rec->Data.MX.pNameExchange, len);
                mx->preference = rec->Data.MX.wPreference;
                dvector_add(vec, mx);
            }

            rec = rec->pNext;
        }

        if (prec) DnsRecordListFree(prec, DNS_TYPE_MX);
    }

#else

    /*~~~~~~~~~~~~~~~~*/
    /* UNIX (IBM) MX resolver */
    u_char  nsbuf[4096];
    /*~~~~~~~~~~~~~~~~*/

    memset(nsbuf, 0, sizeof(nsbuf));

    /*~~~~~~~~~~~~~~~~~~~~*/
    int     l;
    ns_msg  query_parse_msg;
    ns_rr   query_parse_rr;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /* Try to resolve domain */
    res_init();
    l = res_search(domain, ns_c_in, ns_t_mx, nsbuf, sizeof(nsbuf));
    if (l < 0) {

        /* Resolving failed */
        return (NULL);
    }

    ns_initparse(nsbuf, l, &query_parse_msg);

    /*~~*/
    int x;
    /*~~*/

    for (x = 0; x < ns_msg_count(query_parse_msg, ns_s_an); x++) {
        if (ns_parserr(&query_parse_msg, ns_s_an, x, &query_parse_rr)) {
            break;
        }

        if (ns_rr_type(query_parse_rr) == ns_t_mx) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            mxRecord    *mx = malloc(sizeof(mxRecord));
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            mx->preference = ns_get16((u_char *) ns_rr_rdata(query_parse_rr));
            mx->host = calloc(1, 1024);
            if (ns_name_uncompress(ns_msg_base(query_parse_msg), ns_msg_end(query_parse_msg), (u_char *) ns_rr_rdata(query_parse_rr) + 2,
                (char *) mx->host, 1024) < 0) {
                free((char *) mx->host);
                free(mx);
                continue;
            } else dvector_add(vec, mx);
        }
    }
#endif

    /* Fall back to A record if no MX exists */
    if (vec->size == 0) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        struct hostent  *a = gethostbyname(domain);
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if (a) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char            *b;
            size_t          len;
            struct in_addr  x;
            mxRecord        *mx = (mxRecord *) calloc(1, sizeof(mxRecord));
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if (!mx) merror();
            memcpy(&x, a->h_addr_list++, sizeof(x));
            b = inet_ntoa(x);
            len = strlen(b);
            mx->host = (char *) calloc(1, len + 1);
            strncpy((char *) mx->host, b, len);
            mx->preference = 10;
            free(a);
            dvector_add(vec, mx);
        }
    }

    return (vec);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void comm_mxFree(dvector *list) {

    /*~~~~~~~~~~~~~*/
    d_iterator  iter;
    mxRecord    *mx;
    /*~~~~~~~~~~~~~*/

    dforeach((mxRecord *), mx, list, iter) {
        free((char *) mx->host);
        free(mx);
    }

    dvector_destroy(list);
}
