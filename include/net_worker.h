/*
 * =====================================================================================
 *
 *       Filename:  net_worker.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 16时52分26秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#ifndef __LSNET_SERVER_NET_WORKER_H__
#define __LSNET_SERVER_NET_WORKER_H__

#include <pthread.h>
#include "dlist.h"
#include "cserver.h"

class Connection;
class ServerManager;

class NetWorker
{
    private:
        NetWorker(const NetWorker &);
        NetWorker &operator =(const NetWorker &);
    public:
        NetWorker(int num = 1024);
        virtual ~NetWorker();

        void set_servermanager(ServerManager *sm) { _server_manager = sm; }
        ServerManager *get_servermanager() { return _server_manager; }

        void set_id(int8_t id) { _idx = id; }

        int listen(int sock) { return ls_srv_set_listen_fd(_server, sock); }
        void set_idle_timeout(int tm) { ls_srv_set_idle_timeout(_server, tm); }

        bool start();
        void stop() { ls_srv_stop(_server); }
        void join();
        void run() { ls_srv_run(_server); }

        virtual void on_process(Connection *conn);

        void done(Connection *conn);
    protected:
        void done_without_lock(Connection *conn);
    private:
        static int callback(ls_srv_t server);
        static int on_accept(ls_srv_t server, int sock, void **user_args);
        static int on_init(ls_srv_t server, int sock, void *user_args);
        static int on_proc(ls_srv_t server, const netresult_t *net);
        static int on_close(ls_srv_t server, int sock, void *user_args);
    private:
        int8_t _idx;
        bool _running;
        unsigned int _seed;
        struct
        {
            pthread_mutex_t _mutex;
            __dlist_t _queue;
        };
        ServerManager *_server_manager;
    protected:
        pthread_t _thread_id;
        ls_srv_t _server;
};

#endif
