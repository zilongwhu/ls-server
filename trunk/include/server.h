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

#include <string.h>
#include "cserver.h"

class Connection
{
    public:
        Connection()
        {
            _sock_fd = -1;
            ::bzero(&_req_head, sizeof _req_head);
            ::bzero(&_res_head, sizeof _res_head);
            _req_head._magic_num = MAGIC_NUM;
            _res_head._magic_num = MAGIC_NUM;
        }
        virtual ~ Connection() { }

        virtual char *get_req_buf() = 0;
        virtual char *get_res_buf() = 0;
    public:
        int _sock_fd;
        net_head_t _req_head;
        net_head_t _res_head;
};

class Server
{
    private:
        Server(const Server &);
        Server &operator =(const Server &);
    public:
        Server();
        virtual ~Server()
        {
            ls_srv_close(_server);
        }

        int read_timeout() const { return _read_timeout; }
        int write_timeout() const { return _write_timeout; }
        void set_read_timeout(int read_timeout) { _read_timeout = read_timeout; }
        void set_write_timeout(int write_timeout) { _write_timeout = write_timeout; }

        void set_idle_timeout(int idle_timeout)
        {
            ls_srv_set_idle_timeout(_server, idle_timeout);
        }
        int listen(const struct sockaddr *addr, socklen_t addrlen, int backlog)
        {
            return ls_srv_listen(_server, addr, addrlen, backlog);
        }

        void run()
        {
            ls_srv_run(_server);
        }
        void stop()
        {
            ls_srv_stop(_server);
        }

        virtual Connection *on_accept(int sock) = 0;
        virtual int on_init(Connection *conn) = 0;
        virtual int on_process(Connection *conn) = 0;
        virtual int on_timeout(Connection *conn) = 0;
        virtual int on_idle(Connection *conn) = 0;
        virtual int on_error(Connection *conn) = 0;
        virtual int on_peer_close(Connection *conn) = 0;
        virtual int on_close(Connection *conn) = 0;
    protected:
        int _read_timeout;
        int _write_timeout;
        ls_srv_t _server;
};

#endif
