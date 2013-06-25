/*
 * =====================================================================================
 *
 *       Filename:  csrv.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年06月25日 17时17分23秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#include <stdint.h>
#include "log.h"
#include "csrv.h"

static int Server_on_proc(ls_srv_t server, const netresult_t *net)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    Connection *conn = (Connection *)net->_user_ptr2;
    switch (net->_status)
    {
        case NET_ERROR:
            return ps->on_error(conn);
        case NET_EIDLE:
            return ps->on_idle(conn);
        case NET_ETIMEOUT:
            return ps->on_timeout(conn);
        case NET_DONE:
            break;
        default:
            return -1;
    }
    int ret;
    char *buf;
    unsigned int len = 0;
    int sock = net->_sock_fd;
    intptr_t status = (intptr_t)net->_user_ptr;
    NOTICE("status = %d", (int)status);
    switch (net->_op_type)
    {
        case NET_OP_READ:
            if (status == 0)
            {
                status = 1;
                return ls_srv_read(server, sock, &conn->_req_head, sizeof conn->_req_head,
                        (void *)status, ps->read_timeout());
            }
            else if (status == 1)
            {
                buf = conn->get_req_buf(len);
                if (buf == NULL)
                    return -1;
                if (len < conn->_req_head._body_len)
                    return -1;
                status = 2;
                return ls_srv_read(server, sock, buf, conn->_req_head._body_len,
                        (void *)status, ps->read_timeout());
            }
            else if (status == 2)
            {
                ret = ps->on_process(conn);
                if (ret < 0)
                    return -1;
                buf = conn->get_res_buf(len);
                if (buf == NULL)
                    return -1;
                conn->_res_head._body_len = len;
                status = 3;
                return ls_srv_write(server, sock, &conn->_res_head, sizeof conn->_res_head,
                        (void *)status, ps->write_timeout());
            }
            break;
        case NET_OP_WRITE:
            if (status == 3)
            {
                buf = conn->get_res_buf(len);
                if (buf == NULL)
                    return -1;
                if (len != conn->_res_head._body_len)
                    return -1;
                status = 4;
                return ls_srv_write(server, sock, buf, conn->_res_head._body_len,
                        (void *)status, ps->write_timeout());
            }
            else if (status == 4)
            {
                status = 0;
                return ls_srv_read(server, sock, NULL, 0, (void *)status, -1);
            }
            break;
        default:
            return -1;
    }
    return 0;
}
static int Server_on_accept(ls_srv_t server, int sock, void **user_args)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    *user_args = ps->on_accept(sock);
    return 0;
}
static int Server_on_init(ls_srv_t server, int sock, void *user_args)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    Connection *conn = (Connection *)user_args;
    int ret = ps->on_init(conn);
    if (ret < 0)
        return -1;
    intptr_t status = 0;
    return ls_srv_read(server, sock, NULL, 0, (void *)status, -1);
}
static int Server_on_close(ls_srv_t server, int sock, void *user_args)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    return ps->on_close((Connection *)user_args);
}

Server::Server()
{
    _server = ls_srv_create(1024, Server_on_proc, Server_on_accept, Server_on_init, Server_on_close);
    ls_srv_set_userarg(_server, this);
    _read_timeout = _write_timeout = 10;
}
