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
#include <string.h>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "server.h"

void Connection::clear()
{
    _server = NULL;
    _sock_fd = -1;
    _status = ST_NOT_INIT;
    ::bzero(&_req_head, sizeof _req_head);
    ::bzero(&_res_head, sizeof _res_head);
    _req_head._magic_num = MAGIC_NUM;
    _res_head._magic_num = MAGIC_NUM;
}

int Connection::on_init(Server *server, int sock_fd)
{
    if (ST_NOT_INIT != _status && ST_INITED != _status)
    {
        WARNING("conn status=%d, sock[%d], cannot init", (int)_status, sock_fd);
        return -1;
    }
    _server = server;
    _sock_fd = sock_fd;
    _status = ST_INITED;
    return 0;
}

static int Server_callback(ls_srv_t server)
{
    Connection *conn;
    __dlist_t *cur;
    __dlist_t head;
    DLIST_INIT(&head);

    Server *ps = (Server *)ls_srv_get_userarg(server);
    ps->get_processed_conns(head);
    while (!DLIST_EMPTY(&head))
    {
        cur = DLIST_NEXT(&head);
        conn = GET_OWNER(cur, Connection, _list);
        DLIST_REMOVE(cur);
    }
    return 0;
}
static int Server_on_proc(ls_srv_t server, const netresult_t *net)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    Connection *conn = (Connection *)net->_user_ptr2;
    int sock = net->_sock_fd;
    DEBUG("sock[%d], status = %hhd", sock, conn->_status);
    switch (net->_status)
    {
        case NET_ECLOSED:
            TRACE("sock[%d] is closed by peer", sock);
            return conn->on_peer_close();
        case NET_ETIMEOUT:
            if (net->_op_type == NET_OP_READ)
                TRACE("read timeout on sock[%d]", sock);
            else
                TRACE("write timeout on sock[%d]", sock);
            return conn->on_timeout();
        case NET_EIDLE:
            TRACE("sock[%d] becomes idle", sock);
            return conn->on_idle();
        case NET_ERROR:
            TRACE("sock[%d] has error[%s]", sock, strerror_t(net->_errno));
            return conn->on_error();
        case NET_DONE:
            break;
        default:
            WARNING("invalid net status[%hd]", net->_status);
            return -1;
    }
    int ret;
    void *buf;
    switch (net->_op_type)
    {
        case NET_OP_READ:
            switch (conn->_status)
            {
                case Connection::ST_WAITING_REQUEST:
                    conn->_status = Connection::ST_READING_REQUEST_HEAD;
                    TRACE("sock[%d] become readable, try to read conn->_req_head", sock);
                    return ls_srv_read(server, sock, &conn->_req_head, sizeof conn->_req_head, NULL, ps->read_timeout());
                case Connection::ST_READING_REQUEST_HEAD:
                    if (conn->_req_head._magic_num != MAGIC_NUM)
                    {
                        WARNING("failed to chek magic num for sock[%d]", sock);
                        return -1;
                    }
                    buf = conn->get_request_buffer();
                    if (buf == NULL)
                    {
                        WARNING("failed to get request buffer for sock[%d]", sock);
                        return -1;
                    }
                    conn->_status = Connection::ST_READING_REQUEST_BODY;
                    TRACE("read conn->_req_head from sock[%d] ok, try to read req_body[%u]",
                            sock, conn->_req_head._body_len);
                    return ls_srv_read(server, sock, buf, conn->_req_head._body_len, NULL, ps->read_timeout());
                case Connection::ST_READING_REQUEST_BODY:
                    ret = conn->on_process();
                    if (ret < 0)
                    {
                        WARNING("failed to process request for sock[%d]", sock);
                        return -1;
                    }
                    conn->_res_head._magic_num = MAGIC_NUM;
                    conn->_status = Connection::ST_WRITING_RESPONSE_HEAD;
                    TRACE("process request for sock[%d] ok, try to write conn->_res_head", sock);
                    return ls_srv_write(server, sock, &conn->_res_head, sizeof conn->_res_head, NULL, ps->write_timeout());
            }
            WARNING("invalid status[%hhd] for sock[%d] when op_type=NET_OP_READ", conn->_status, sock);
            return -1;
        case NET_OP_WRITE:
            switch (conn->_status)
            {
                case Connection::ST_WRITING_RESPONSE_HEAD:
                    buf = conn->get_response_buffer();
                    if (buf == NULL)
                    {
                        WARNING("failed to get response buffer for sock[%d]", sock);
                        return -1;
                    }
                    conn->_status = Connection::ST_WRITING_RESPONSE_BODY;
                    TRACE("send conn->_res_head to sock[%d] ok, try to send res_body[%u]", sock, conn->_res_head._body_len);
                    return ls_srv_write(server, sock, buf, conn->_res_head._body_len, NULL, ps->write_timeout());
                case Connection::ST_WRITING_RESPONSE_BODY:
                    conn->_status = Connection::ST_WAITING_REQUEST;
                    TRACE("send res_body to sock[%d] ok, waiting for request again", sock);
                    return ls_srv_read(server, sock, NULL, 0, NULL, -1);
            }
            WARNING("invalid status[%hhd] for sock[%d] when op_type=NET_OP_WRITE", conn->_status, sock);
            return -1;
    }
    WARNING("unexpected op_type[%hd] on sock[%d]", net->_op_type, sock);
    return -1;
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
    int ret = conn->on_init(ps, sock);
    if (ret < 0)
    {
        WARNING("failed to init conn for sock[%d], ret=%d", sock, ret);
        return -1;
    }
    conn->_status = Connection::ST_WAITING_REQUEST;
    TRACE("waiting for request on sock[%d]", sock);
    return ls_srv_read(server, sock, NULL, 0, NULL, -1);
}
static int Server_on_close(ls_srv_t server, int sock, void *user_args)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
    Connection *conn = (Connection *)user_args;
    int ret = conn->on_close();
    ps->free_conn(conn);
    return ret;
}

Server::Server(int sock_num_hint)
{
    _sock_num_hint = sock_num_hint;
    if (_sock_num_hint <= 1024)
        _sock_num_hint = 1024;
    _server = ls_srv_create(_sock_num_hint, Server_on_proc, Server_on_accept, Server_on_init, Server_on_close);
    if (_server)
    {
        ls_srv_set_userarg(_server, this);
        ls_srv_set_callback(_server, Server_callback);
    }
    _idle_timeout = _read_timeout = _write_timeout = -1;
    pthread_mutex_init(&_mutex, NULL);
    DLIST_INIT(&_queue);
}

Server::~Server()
{
    if (_server)
        ls_srv_close(_server);
    _sock_num_hint = 0;
    _idle_timeout = _read_timeout = _write_timeout = -1;
    pthread_mutex_destroy(&_mutex);
    DLIST_REMOVE(&_queue);
}

void Server::append_conn(Connection *conn)
{
    if (!conn) return ;
    DLIST_REMOVE(&conn->_list);
    pthread_mutex_lock(&_mutex);
    DLIST_INSERT_B(&conn->_list, &_queue);
    pthread_mutex_unlock(&_mutex);
}

void Server::get_processed_conns(__dlist_t &conns)
{
    DLIST_INIT(&conns);
    __dlist_t *next = NULL;
    pthread_mutex_lock(&_mutex);
    if (!DLIST_EMPTY(&_queue))
    {
        next = DLIST_NEXT(&_queue);
        DLIST_REMOVE(&_queue);
        DLIST_INSERT_B(&conns, next);
    }
    pthread_mutex_unlock(&_mutex);
}
