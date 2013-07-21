/*
 * =====================================================================================
 *
 *       Filename:  worker.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 18时10分53秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#ifndef __LSNET_SERVER_WORKER_H__
#define __LSNET_SERVER_WORKER_H__

#include <pthread.h>
#include "dlist.h"

class Connection;
class ServerManager;

class Worker
{
    private:
        Worker(const Worker &);
        Worker &operator =(const Worker &);
    public:
        Worker();
        ~Worker();

        void set_servermanager(ServerManager *sm) { _server_manager = sm; }

        bool start();
        void stop();
        void join();
        void run();

        void post(Connection *conn);
    private:
        bool _running;
        bool _stopped;
        ServerManager *_server_manager;
        pthread_t _thread_id;
        pthread_mutex_t _mutex;
        pthread_cond_t _cond;
        __dlist_t _queue;
};

#endif
