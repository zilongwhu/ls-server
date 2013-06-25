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
#include "csrv.h"

static int Server_on_proc(ls_srv_t server, const netresult_t *net)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    Connection *conn = (Connection *)net->_user_ptr;
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
    switch (net->_op_type)
    {
        case NET_OP_READ:
            break;
        case NET_OP_WRITE:
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
    return ps->on_init((Connection *)user_args);
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
}

Connection *Server::on_accept(int sock)
{
    return NULL;
}
int Server::on_init(Connection *conn)
{
    return 0;
}
int Server::on_process(Connection *conn)
{
    return 0;
}
int Server::on_timeout(Connection *conn)
{
    return 0;
}
int Server::on_idle(Connection *conn)
{
    return 0;
}
int Server::on_error(Connection *conn)
{
    return 0;
}
int Server::on_close(Connection *conn)
{
    return 0;
}
