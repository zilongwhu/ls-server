/*
 * =====================================================================================
 *
 *       Filename:  server_manager.h
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

#ifndef __LSNET_SERVER_MANAGER_H__
#define __LSNET_SERVER_MANAGER_H__

#include <stdint.h>
#include "worker.h"
#include "net_worker.h"
#include "connection.h"

class ServerManager
{
    private:
        ServerManager(const ServerManager &);
        ServerManager &operator =(const ServerManager &);
    public:
        ServerManager();
        virtual ~ServerManager();

        int read_timeout() const { return _read_timeout; }
        int write_timeout() const { return _write_timeout; }
        int idle_timeout() const { return _idle_timeout; }
        bool is_short() const { return _is_short; }

        void set_read_timeout(int read_timeout) { _read_timeout = read_timeout; }
        void set_write_timeout(int write_timeout) { _write_timeout = write_timeout; }
        void set_idle_timeout(int idle_timeout) { _idle_timeout = idle_timeout; }
        void set_short_only() { _is_short = true; }
        void set_sock_num(int sock_num) { _sock_num_hint = sock_num; }

        int init(int worker_num, int net_worker_num = 1);
        int run(const struct sockaddr *addr, socklen_t addrlen, int backlog);
        void stop();

        int get_worker_num() { return _worker_num; }
        int get_net_worker_num() { return _net_worker_num; }

        virtual Connection *on_accept(int sock) = 0;
        virtual void free_conn(Connection *conn) = 0;

        void post(Connection *conn); /* read request ok, post conn to worker */
        void done(Connection *conn); /* request has been processed, need send response */
    protected:
        struct
        {
            int _idle_timeout;
            int _read_timeout;
            int _write_timeout;

            bool _is_short;

            int _sock_num_hint;

            int _worker_num;
            int _net_worker_num;
        };

        int _listen_fd;
        NetWorker *_nets;

        Worker *_workers;
};

#endif
