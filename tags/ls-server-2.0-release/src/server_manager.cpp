/*
 * =====================================================================================
 *
 *       Filename:  server_manager.cpp
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
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <new>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "server_manager.h"

ServerManager::ServerManager()
{
    _idle_timeout = _read_timeout = _write_timeout = -1;
    _is_short = false;
    _sock_num_hint = 1024;
    _worker_num = 5;
    _net_worker_num = 1;
    _listen_fd = -1;
    _nets = NULL;
    _workers = NULL;
}

ServerManager::~ServerManager()
{
    if (_nets)
    {
        for (int i = 0; i < _net_worker_num; ++i)
        {
            _nets[i].~NetWorker();
        }
        free(_nets);
        _nets = NULL;
    }
    if (_workers)
    {
        delete [] _workers;
        _workers = NULL;
    }
    _idle_timeout = _read_timeout = _write_timeout = -1;
    _is_short = false;
    _sock_num_hint = 0;
    _worker_num = 0;
    _net_worker_num = 0;
    SAFE_CLOSE(_listen_fd);
    _listen_fd = -1;
}

int ServerManager::init(int worker_num, int net_worker_num)
{
    if (worker_num <= 0)
        worker_num = 1;
    if (worker_num >= 0x7FFF)
        worker_num = 0x7FFF;
    if (net_worker_num <= 0)
        net_worker_num = 1;
    if (net_worker_num >= 0x7F)
        net_worker_num = 0x7F;
    if (_nets || _workers)
    {
        WARNING("already init, cannot do again.");
        return -1;
    }
    NOTICE("worker_num=%d, net_worker_num=%d.", worker_num, net_worker_num);
    _nets = (NetWorker *)malloc(sizeof(NetWorker)*net_worker_num);
    if (NULL == _nets)
    {
        WARNING("failed to alloc mem for net_workers[%d].", net_worker_num);
        return -1;
    }
    for (int i = 0; i < net_worker_num; ++i)
    {
        new (_nets + i) NetWorker(_sock_num_hint);
        _nets[i].set_id(i);
        _nets[i].set_servermanager(this);
    }
    _workers = new(std::nothrow) Worker[worker_num];
    if (NULL == _workers)
    {
        for (int i = 0; i < net_worker_num; ++i)
        {
            _nets[i].~NetWorker();
        }
        free(_nets);
        _nets = NULL;
        WARNING("failed to new Workers[%d].", worker_num);
        return -1;
    }
    for (int i = 0; i < worker_num; ++i)
    {
        _workers[i].set_servermanager(this);
    }
    _worker_num = worker_num;
    _net_worker_num = net_worker_num;
    NOTICE("init ok, worker_num=%d, net_worker_num=%d.", _worker_num, _net_worker_num);
    return 0;
}

int ServerManager::run(const struct sockaddr *addr, socklen_t addrlen, int backlog)
{
    if (!(_nets && _workers))
    {
        WARNING("not init yet.");
        return -1;
    }
    if (_listen_fd >= 0)
    {
        WARNING("server is already running.");
        return 0;
    }
    _listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if ( _listen_fd < 0 )
    {
        WARNING("failed to create socket, error[%d].", errno);
        return -1;
    }
    if ( ::bind(_listen_fd, addr, addrlen) < 0 )
    {
        WARNING("failed to bind socket[%d], error[%d].", _listen_fd, errno);
        SAFE_CLOSE(_listen_fd);
        _listen_fd = -1;
        return -1;
    }
    if ( ::listen(_listen_fd, backlog) < 0 )
    {
        WARNING("failed to listen sock[%d], error[%d].", _listen_fd, errno);
        SAFE_CLOSE(_listen_fd);
        _listen_fd = -1;
        return -1;
    }
    int i;
    for (i = 0; i < _worker_num; ++i)
    {
        if (!_workers[i].start())
        {
            WARNING("failed to start worker[%d].", i);
            goto FAIL;
        }
    }
    for (i = 0; i < _net_worker_num; ++i)
    {
        _nets[i].listen(_listen_fd);
        _nets[i].set_idle_timeout(_idle_timeout);
        if (!_nets[i].start())
        {
            WARNING("failed to start net_worker[%d].", i);
            goto FAIL;
        }
    }
    if (0)
    {
FAIL:
        this->stop();
        return -1;
    }
    NOTICE("start server ok, worker_num[%d], net_worker_num[%d].", _worker_num, _net_worker_num);
    return 0;
}

void ServerManager::stop()
{
    if (_nets)
    {
        for (int i = 0; i < _net_worker_num; ++i)
        {
            _nets[i].stop();
            _nets[i].join();
        }
    }
    if (_workers)
    {
        for (int i = 0; i < _worker_num; ++i)
        {
            _workers[i].stop();
            _workers[i].join();
        }
    }
    SAFE_CLOSE(_listen_fd);
    _listen_fd = -1;
}

void ServerManager::post(Connection *conn)
{
    if (conn)
    {
        _workers[conn->_idx % _worker_num].post(conn);
    }
}

void ServerManager::done(Connection *conn)
{
    if (conn)
    {
        _nets[conn->_net_idx].done(conn);
    }
}
