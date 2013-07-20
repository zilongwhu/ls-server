/*
 * =====================================================================================
 *
 *       Filename:  net_worker.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 17时17分55秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "net_worker.h"
#include "connection.h"
#include "server_manager.h"

int NetWorker::callback(ls_srv_t server)
{
    int sock;
    Connection *conn;
    __dlist_t *cur;
    __dlist_t head;
    DLIST_INIT(&head);

    NetWorker *nw = (NetWorker *)ls_srv_get_userarg(server);
    ServerManager *ps = nw->get_servermanager();
    {
        __dlist_t *next = NULL;
        pthread_mutex_lock(&nw->_mutex);
        if (!DLIST_EMPTY(&nw->_queue))
        {
            next = DLIST_NEXT(&nw->_queue);
            DLIST_REMOVE(&nw->_queue);
        }
        pthread_mutex_unlock(&nw->_mutex);
        if (next)
        {
            DLIST_INSERT_B(&head, next);
        }
    }
    while (!DLIST_EMPTY(&head))
    {
        cur = DLIST_NEXT(&head);
        conn = GET_OWNER(cur, Connection, _list);
        DLIST_REMOVE(cur);

        sock = conn->_sock_fd;
        if (ls_srv_enable_notify(server, sock) < 0)
        {
            FATAL("sock[%d] is processed, but failed to enable notification", sock);
        }
        if (conn->_status == Connection::ST_PROCESSING_REQUEST_OK)
        {
            conn->_res_head._magic_num = MAGIC_NUM;
            conn->_status = Connection::ST_WRITING_RESPONSE_HEAD;
            TRACE("process request for sock[%d] ok, try to write conn->_res_head", sock);
            ls_srv_write(server, sock, &conn->_res_head, sizeof conn->_res_head,
                    NULL, ps->write_timeout());
        }
        else
        {
            TRACE("failed to process request for sock[%d], closing it", sock);
            ls_srv_close_sock(server, sock);
        }
    }
    return 0;
}

int NetWorker::on_proc(ls_srv_t server, const netresult_t *net)
{
    NetWorker *nw = (NetWorker *)ls_srv_get_userarg(server);
    ServerManager *ps = nw->get_servermanager();
    Connection *conn = (Connection *)net->_user_ptr2;
    int sock = net->_sock_fd;
    DEBUG("sock[%d], status = %hhd", sock, conn->_status);
    switch (net->_status)
    {
        case NET_EIDLE:
            TRACE("sock[%d] becomes idle", sock);
            conn->on_idle();
            return 0;
        case NET_ERROR:
            TRACE("sock[%d] has error[%s]", sock, strerror_t(net->_errno));
            conn->on_error();
            return 0;
        case NET_ECLOSED:
            TRACE("sock[%d] is closed by peer", sock);
            return conn->on_peer_close();
        case NET_ETIMEOUT:
            if (net->_op_type == NET_OP_READ)
                TRACE("read timeout on sock[%d]", sock);
            else
                TRACE("write timeout on sock[%d]", sock);
            conn->on_timeout();
            return -1;
        case NET_DONE:
            break;
        default:
            WARNING("invalid net status[%hd]", net->_status);
            return -1;
    }
    void *buf;
    switch (net->_op_type)
    {
        case NET_OP_READ:
            switch (conn->_status)
            {
                case Connection::ST_WAITING_REQUEST:
                    conn->_status = Connection::ST_READING_REQUEST_HEAD;
                    TRACE("sock[%d] become readable, try to read conn->_req_head", sock);
                    return ls_srv_read(server, sock, &conn->_req_head, sizeof conn->_req_head,
                            NULL, ps->read_timeout());
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
                    return ls_srv_read(server, sock, buf, conn->_req_head._body_len,
                            NULL, ps->read_timeout());
                case Connection::ST_READING_REQUEST_BODY:
                    conn->_status = Connection::ST_PROCESSING_REQUEST;
                    TRACE("read req_body[%u] for sock[%d] ok, try to process conn",
                            conn->_req_head._body_len, sock);
                    if (ls_srv_disable_notify(server, sock) < 0)
                    {
                        WARNING("failed to disable notification for sock[%d]", sock);
                        return -1;
                    }
                    nw->on_process(conn);
                    return 0;
            }
            WARNING("invalid status[%hhd] for sock[%d] when op_type=NET_OP_READ",
                    conn->_status, sock);
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
                    TRACE("send conn->_res_head to sock[%d] ok, try to send res_body[%u]",
                            sock, conn->_res_head._body_len);
                    return ls_srv_write(server, sock, buf, conn->_res_head._body_len,
                            NULL, ps->write_timeout());
                case Connection::ST_WRITING_RESPONSE_BODY:
                    conn->_status = Connection::ST_WAITING_REQUEST;
                    TRACE("send res_body to sock[%d] ok, waiting for request again", sock);
                    return ls_srv_read(server, sock, NULL, 0, NULL, -1);
            }
            WARNING("invalid status[%hhd] for sock[%d] when op_type=NET_OP_WRITE",
                    conn->_status, sock);
            return -1;
    }
    WARNING("unexpected op_type[%hd] on sock[%d]", net->_op_type, sock);
    return -1;
}

int NetWorker::on_accept(ls_srv_t server, int sock, void **user_args)
{
    NetWorker *nw = (NetWorker *)ls_srv_get_userarg(server);
    ServerManager *ps = nw->get_servermanager();
    if ((*user_args = ps->on_accept(sock)))
        return 0;
    return -1;
}

int NetWorker::on_init(ls_srv_t server, int sock, void *user_args)
{
    NetWorker *nw = (NetWorker *)ls_srv_get_userarg(server);
    ServerManager *ps = nw->get_servermanager();
    Connection *conn = (Connection *)user_args;

    conn->_sock_fd = sock;
    conn->_idx = rand_r(&nw->_seed) % ps->get_worker_num();
    conn->_net_idx = nw->_idx;
    conn->_status = Connection::ST_INITED;

    int ret = conn->on_init();
    if (ret < 0)
    {
        WARNING("failed to init conn for sock[%d], ret=%d", sock, ret);
        return -1;
    }
    conn->_status = Connection::ST_WAITING_REQUEST;
    TRACE("waiting for request on sock[%d]", sock);
    return ls_srv_read(server, sock, NULL, 0, NULL, -1);
}

int NetWorker::on_close(ls_srv_t server, int sock, void *user_args)
{
    NetWorker *nw = (NetWorker *)ls_srv_get_userarg(server);
    ServerManager *ps = nw->get_servermanager();
    Connection *conn = (Connection *)user_args;

    if (conn)
    {
        conn->on_close();
        ps->free_conn(conn);
    }
    return 0;
}

NetWorker::NetWorker(int num)
{
    if (num < 1024)
        num = 1024;
    _server = ls_srv_create(num, on_proc, on_accept, on_init, on_close);
    ls_srv_set_userarg(_server, this);
    ls_srv_set_callback(_server, callback);
    pthread_mutex_init(&_mutex, NULL);
    DLIST_INIT(&_queue);
    _idx = 0;
    _seed = rand() * rand();
    _running = false;
    _thread_id = 0;
}

NetWorker::~NetWorker()
{
    ls_srv_close(_server);
    _server = NULL;
    _server_manager = NULL;
    pthread_mutex_destroy(&_mutex);
    DLIST_REMOVE(&_queue);
    _idx = _seed = 0;
    _running = false;
    _thread_id = 0;
}

void NetWorker::on_process(Connection *conn)
{
    if (_server_manager && conn)
        _server_manager->post(conn);
}

void NetWorker::done(Connection *conn)
{
    if (!conn) return ;
    DLIST_REMOVE(&conn->_list);
    pthread_mutex_lock(&_mutex);
    DLIST_INSERT_B(&conn->_list, &_queue);
    pthread_mutex_unlock(&_mutex);
}

void NetWorker::done_without_lock(Connection *conn)
{
    if (!conn) return ;
    DLIST_REMOVE(&conn->_list);
    DLIST_INSERT_B(&conn->_list, &_queue);
}

static void *net_worker_run(void *arg)
{
    NetWorker *nw = (NetWorker *)arg;
    nw->run();
    return NULL;
}

bool NetWorker::start()
{
    if (_running) return true;
    int ret = pthread_create(&_thread_id, NULL, net_worker_run, this);
    if (ret)
    {
        WARNING("failed to create net_worker thread, err=%d.", ret);
        return false;
    }
    _running = true;
    return true;
}

void NetWorker::join()
{
    if (_running)
    {
        pthread_join(_thread_id, NULL);
        _running = false;
    }
}
