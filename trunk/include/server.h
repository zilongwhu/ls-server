/*
 * =====================================================================================
 *
 *       Filename:  csrv.h
 *
 *    Description:  cpp wrapper
 *
 *        Version:  1.0
 *        Created:  2013年06月25日 15时43分34秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#ifndef __LS_CPP_SERVER_H__
#define __LS_CPP_SERVER_H__

#include <stdint.h>
#include <pthread.h>
#include "dlist.h"
#include "cserver.h"

class Server;

class Connection
{
    public:
        Connection()
        {
            DLIST_INIT(&_list);
            this->clear();
        }
        virtual ~ Connection()
        {
            DLIST_REMOVE(&_list);
            this->clear();
        }

        void clear();

        virtual void *get_request_buffer() = 0;
        virtual void *get_response_buffer() = 0;

        virtual int on_init(Server *server, int sock_fd);

        virtual int on_process() = 0;
        virtual int on_timeout() = 0;
        virtual int on_idle() = 0;
        virtual int on_error() = 0;
        virtual int on_peer_close() = 0;
        virtual int on_close() = 0;
    public:
        enum
        {
            ST_NOT_INIT = 0,
            ST_INITED,
            ST_WAITING_REQUEST,
            ST_READING_REQUEST_HEAD,
            ST_READING_REQUEST_BODY,
            ST_PROCESSING_REQUEST,
            ST_WRITING_RESPONSE_HEAD,
            ST_WRITING_RESPONSE_BODY,
        };

        int _sock_fd;
        int8_t _status;
        net_head_t _req_head;
        net_head_t _res_head;

        __dlist_t _list;

        Server *_server;
};

class Server
{
    private:
        Server(const Server &);
        Server &operator =(const Server &);
    public:
        Server(int sock_num_hint = 1024);
        virtual ~Server();

        int read_timeout() const { return _read_timeout; }
        int write_timeout() const { return _write_timeout; }
        void set_read_timeout(int read_timeout) { _read_timeout = read_timeout; }
        void set_write_timeout(int write_timeout) { _write_timeout = write_timeout; }

        int idle_timeout() const { return _idle_timeout; }
        void set_idle_timeout(int idle_timeout)
        {
            _idle_timeout = idle_timeout;
            ls_srv_set_idle_timeout(_server, _idle_timeout);
        }

        int listen(const struct sockaddr *addr, socklen_t addrlen, int backlog)
        {
            return ls_srv_listen(_server, addr, addrlen, backlog);
        }

        void run() { ls_srv_run(_server); }
        void stop() { ls_srv_stop(_server); }

        virtual Connection *on_accept(int sock) = 0;
        virtual void free_conn(Connection *conn) = 0;

        void append_conn(Connection *conn);
        void get_processed_conns(__dlist_t &conns);
    protected:
        struct
        {
            int _idle_timeout;
            int _read_timeout;
            int _write_timeout;
            int _sock_num_hint;
        };
        struct
        {
            pthread_mutex_t _mutex;
            __dlist_t _queue;
        };
        ls_srv_t _server;
};

#endif
