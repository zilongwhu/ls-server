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
#include <new>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "server.h"

class Worker
{
    private:
        Worker(const Worker &);
        Worker &operator =(const Worker &);
    public:
        Worker();
        ~Worker();

        void set_server(Server *server) { _server = server; }

        bool start();
        void stop();
        void join();
        void run();

        void append_conn(Connection *conn);
    private:
        bool _running;
        bool _stopped;
        Server *_server;
        pthread_t _thread_id;
        pthread_mutex_t _mutex;
        pthread_cond_t _cond;
        __dlist_t _queue;
};

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
    int sock;
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
static int Server_on_proc(ls_srv_t server, const netresult_t *net)
{
    Server *ps = (Server *)ls_srv_get_userarg(server);
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
                    ps->on_process(conn);
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
    if (conn)
    {
        conn->on_close();
        ps->free_conn(conn);
    }
    return 0;
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
    _worker_num = 3;
    _running_worker_num = 0;
    _workers = NULL;
    pthread_mutex_init(&_mutex, NULL);
    DLIST_INIT(&_queue);
}

Server::~Server()
{
    if (_server)
        ls_srv_close(_server);
    _sock_num_hint = 0;
    _idle_timeout = _read_timeout = _write_timeout = -1;
    _worker_num = 0;
    _running_worker_num = 0;
    _workers = NULL;
    pthread_mutex_destroy(&_mutex);
    DLIST_REMOVE(&_queue);
}

void Server::run(int worker_num)
{
    if (_running_worker_num > 0)
        return ;
    if (worker_num <= 0)
        worker_num = 3;
    _workers = new(std::nothrow) Worker[worker_num];
    if (NULL == _workers)
    {
        WARNING("failed to new Workers[%d].", worker_num);
        return ;
    }
    for (_running_worker_num = 0;
            _running_worker_num < worker_num;
            ++_running_worker_num)
    {
        _workers[_running_worker_num].set_server(this);
        if (!_workers[_running_worker_num].start())
        {
            WARNING("failed to start worker[%d], need start %d workers.",
                    _running_worker_num, _worker_num);
            break;
        }
    }
    if (_running_worker_num <= 0)
    {
        delete [] _workers;
        _workers = NULL;
        WARNING("failed to start Workers[%d].", worker_num);
        return ;
    }
    _worker_num = worker_num;
    ls_srv_run(_server);
    NOTICE("start server ok, worker_num[%d], running_worker_num[%d].",
            _worker_num, _running_worker_num);
}

void Server::stop()
{
    if (_running_worker_num <= 0)
        return ;
    ls_srv_stop(_server);
    if (_workers)
    {
        for (int i = 0; i < _running_worker_num; ++i)
        {
            _workers[i].stop();
            _workers[i].join();
        }
        delete [] _workers;
        _workers = NULL;
    }
    _running_worker_num = 0;
}

void Server::on_process(Connection *conn)
{
    if (_running_worker_num > 0)
    {
        _workers[conn->_sock_fd%_running_worker_num].append_conn(conn);
    }
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
    }
    pthread_mutex_unlock(&_mutex);
    if (next)
    {
        DLIST_INSERT_B(&conns, next);
    }
}

Worker::Worker()
{
    _thread_id = 0;
    _running = false;
    _stopped = false;
    _server = NULL;
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_cond, NULL);
    DLIST_INIT(&_queue);
}

Worker::~Worker()
{
    _thread_id = 0;
    _running = false;
    _stopped = false;
    _server = NULL;
    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
    DLIST_REMOVE(&_queue);
}

static void *worker_run(void *arg)
{
    Worker *worker = (Worker *)arg;
    worker->run();
    return NULL;
}

bool Worker::start()
{
    if (_running) return true;
    _stopped = false;
    int ret = pthread_create(&_thread_id, NULL, worker_run, this);
    if (ret)
    {
        WARNING("failed to create worker thread, err=%d.", ret);
        return false;
    }
    _running = true;
    return true;
}

void Worker::stop()
{
    _stopped = true;
}

void Worker::join()
{
    pthread_join(_thread_id, NULL);
    _running = false;
}

void Worker::append_conn(Connection *conn)
{
    if (!conn) return ;
    DLIST_REMOVE(&conn->_list);
    int empty = 0;
    pthread_mutex_lock(&_mutex);
    if (DLIST_EMPTY(&_queue)) empty = 1;
    DLIST_INSERT_B(&conn->_list, &_queue);
    if (empty) pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_mutex);
}

void Worker::run()
{
    int ret;
    __dlist_t head;
    __dlist_t *next;
    Connection *conn;
    NOTICE("worker is running now.");
    while (!_stopped)
    {
        DLIST_INIT(&head);
        pthread_mutex_lock(&_mutex);
        while (DLIST_EMPTY(&_queue))
            pthread_cond_wait(&_cond, &_mutex);
        next = DLIST_NEXT(&_queue);
        DLIST_REMOVE(&_queue);
        pthread_mutex_unlock(&_mutex);
        DLIST_INSERT_B(&head, next);
        while (!DLIST_EMPTY(&head))
        {
            next = DLIST_NEXT(&head);
            DLIST_REMOVE(next);
            conn = GET_OWNER(next, Connection, _list);
            TRACE("start to process conn, sock[%d].", conn->_sock_fd);
            try
            {
                ret = conn->on_process();
                if (ret == 0)
                {
                    conn->_status = Connection::ST_PROCESSING_REQUEST_OK;
                    TRACE("process conn ok, sock[%d].", conn->_sock_fd);
                }
                else
                {
                    conn->_status = Connection::ST_PROCESSING_REQUEST_FAIL;
                    WARNING("failed to process conn, sock[%d], ret=%d.", conn->_sock_fd, ret);
                }
            }
            catch (...)
            {
                WARNING("failed to process conn, sock[%d], exception catched.", conn->_sock_fd);
            }
            _server->append_conn(conn);
        }
    }
    NOTICE("worker is stopped by user, exiting now.");
}
