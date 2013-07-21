/*
 * =====================================================================================
 *
 *       Filename:  worker.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 18时18分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include "log.h"
#include "utils.h"
#include "error.h"
#include "worker.h"
#include "connection.h"
#include "server_manager.h"

Worker::Worker()
{
    _thread_id = 0;
    _running = false;
    _stopped = false;
    _server_manager = NULL;
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_cond, NULL);
    DLIST_INIT(&_queue);
}

Worker::~Worker()
{
    _thread_id = 0;
    _running = false;
    _stopped = false;
    _server_manager = NULL;
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
        WARNING("failed to create worker thread, err=[%d, %s].", ret, strerror_t(ret));
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
    if (_running)
    {
        pthread_join(_thread_id, NULL);
        _running = false;
    }
}

void Worker::post(Connection *conn)
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
            _server_manager->done(conn);
        }
    }
    NOTICE("worker is stopped by user, exiting now.");
}
