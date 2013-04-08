/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年04月08日 21时29分25秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "log.h"
#include "server.h"
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

enum
{
    REQ_LEN,
    REQ_BODY,
    RES_LEN,
    RES_BODY,
};

struct conn
{
    int _state;
    int _in_len;
    char _in[256];
    int _out_len;
    char _out[256];
};

int proc(ls_srv_t server, const netresult_t *res)
{
    struct conn *p = (struct conn *)res->_user_ptr2;
    switch (res->_op_type)
    {
        case NET_OP_READ:
            if (p->_state == REQ_LEN)
            {
                NOTICE("read len[%d] from sock[%d] ok", p->_in_len, res->_sock_fd);
                if (ls_srv_read(server, res->_sock_fd, p->_in, p->_in_len, NULL, 10) < 0)
                {
                    return -1;
                }
                p->_state = REQ_BODY;
            }
            else
            {
                NOTICE("read body from sock[%d] ok", res->_sock_fd);
                int i;
                for (i = 0; i < p->_in_len; ++i)
                {
                    p->_out[i] = tolower(p->_in[i]);
                }
                p->_out_len = p->_in_len;
                if (ls_srv_write(server, res->_sock_fd, &p->_out_len, sizeof p->_out_len, NULL, 10) < 0)
                {
                    return -1;
                }
                p->_state = RES_LEN;
            }
            break;
        case NET_OP_WRITE:
            if (p->_state == RES_LEN)
            {
                NOTICE("write len[%d] to sock[%d] ok", p->_out_len, res->_sock_fd);
                if (ls_srv_write(server, res->_sock_fd, p->_out, p->_out_len, NULL, 10) < 0)
                {
                    return -1;
                }
                p->_state = RES_BODY;
            }
            else
            {
                NOTICE("write body to sock[%d] ok", res->_sock_fd);
                return -1;
            }
            break;
        default:
            return -1;
    }
    return 0;
}

int on_accept(ls_srv_t server, int sock, void **user_args)
{
    *user_args = calloc(1, sizeof(struct conn));
    if (NULL == *user_args)
    {
        WARNING("failed to create conn");
        return -1;
    }
    NOTICE("accept sock[%d] ok, conn[%p]", sock, *user_args);
    return 0;
}

int on_init(ls_srv_t server, int sock, void *user_args)
{
    struct conn *p = (struct conn *)user_args;
    if (0 == ls_srv_read(server, sock, &p->_in_len, sizeof p->_in_len, NULL, 10))
    {
        p->_state = REQ_LEN;
        return 0;
    }
    return -1;
}

int on_close(ls_srv_t server, int sock, void *user_args)
{
    NOTICE("sock[%d] is closing, free conn", sock);
    free(user_args);
    return 0;
}

int main(int argc, char *argv[])
{
    ls_srv_t server = ls_srv_create(1024, proc, on_accept, on_init, on_close);

    struct sockaddr_in addr;
    bzero(&addr, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7654);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ls_srv_listen(server, (struct sockaddr *)&addr, sizeof addr, 5);
    ls_srv_run(server);
    ls_srv_close(server);

    return 0;
}
