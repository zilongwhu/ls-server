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
        ServerManager(int net_worker_num, int sock_num_hint = 1024);
        virtual ~ServerManager();

        int read_timeout() const { return _read_timeout; }
        int write_timeout() const { return _write_timeout; }
        int idle_timeout() const { return _idle_timeout; }

        void set_read_timeout(int read_timeout) { _read_timeout = read_timeout; }
        void set_write_timeout(int write_timeout) { _write_timeout = write_timeout; }
        void set_idle_timeout(int idle_timeout) { _idle_timeout = idle_timeout; }

        int listen(const struct sockaddr *addr, socklen_t addrlen, int backlog);
        void run(int worker_num = 3);
        void stop();

        int get_worker_num() { return _running_worker_num; }
        int get_net_worker_num() { return _net_worker_num; }

        virtual Connection *on_accept(int sock) = 0;
        virtual void free_conn(Connection *conn) = 0;

        void post(Connection *conn);
        void done(Connection *conn);
    protected:
        struct
        {
            int _idle_timeout;
            int _read_timeout;
            int _write_timeout;

            int _sock_num_hint;

            int _worker_num;
            int _net_worker_num;
        };

        int _listen_fd;
        NetWorker *_nets;

        int _running_worker_num;
        Worker *_workers;
};

#endif
