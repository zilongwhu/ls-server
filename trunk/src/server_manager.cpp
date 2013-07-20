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

ServerManager::ServerManager(int net_worker_num, int sock_num_hint)
{
    _net_worker_num = net_worker_num;
    if (_net_worker_num < 1)
        _net_worker_num = 1;
    if (_net_worker_num > 127)
        _net_worker_num = 127;
    _sock_num_hint = sock_num_hint;
    if (_sock_num_hint < 1024)
        _sock_num_hint = 1024;
    _idle_timeout = _read_timeout = _write_timeout = -1;
    _worker_num = 3;
    _running_worker_num = 0;
    _listen_fd = -1;
    _nets = (NetWorker *)malloc(sizeof(NetWorker)*_net_worker_num);
    if (_nets)
    {
        for (int i = 0; i < _net_worker_num; ++i)
        {
            new (_nets + i) NetWorker(_sock_num_hint);
        }
    }
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
        _nets = NULL;
    }
    _net_worker_num = 0;
    _sock_num_hint = 0;
    _idle_timeout = _read_timeout = _write_timeout = -1;
    _worker_num = 0;
    _running_worker_num = 0;
    SAFE_CLOSE(_listen_fd);
    _listen_fd = -1;
    _workers = NULL;
}

int ServerManager::listen(const struct sockaddr *addr, socklen_t addrlen, int backlog)
{
    if (NULL == _nets)
    {
        WARNING("not init net_workers.");
        return -1;
    }
    if (_listen_fd >= 0)
        return 0;
    _listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if ( _listen_fd < 0 )
    {
        WARNING("failed to create socket, error[%d].", errno);
        return -1;
    }
    if ( ::bind(_listen_fd, addr, addrlen) < 0 )
    {
        WARNING("failed to bind socket, error[%d].", errno);
        goto FAIL;
    }
    if ( ::listen(_listen_fd, backlog) < 0 )
    {
        WARNING("failed to listen sock[%d], error[%d].", _listen_fd, errno);
        goto FAIL;
    }
    for (int i = 0; i < _net_worker_num; ++i)
    {
        _nets[i].listen(_listen_fd);
    }
    return 0;
FAIL:
    SAFE_CLOSE(_listen_fd);
    _listen_fd = -1;
    return -1;
}

void ServerManager::run(int worker_num)
{
    if (NULL == _nets)
    {
        WARNING("not init net_workers.");
        return ;
    }
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
        _workers[_running_worker_num].set_servermanager(this);
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
    int i;
    for (i = 0; i < _net_worker_num; ++i)
    {
        _nets[i].set_id(i);
        _nets[i].set_servermanager(this);
        _nets[i].set_idle_timeout(_idle_timeout);
        if (!_nets[i].start())
        {
            WARNING("failed to start net_worker[%d].", i);
            break;
        }
    }
    if (i <= 0)
    {
        this->stop();
        return ;
    }
    NOTICE("start server ok, net_worker_num[%d], worker_num[%d], running_worker_num[%d].",
            i, _worker_num, _running_worker_num);
}

void ServerManager::stop()
{
    if (_running_worker_num <= 0)
        return ;
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

void ServerManager::post(Connection *conn)
{
    if (conn && _running_worker_num > 0)
    {
        _workers[conn->_idx % _running_worker_num].post(conn);
    }
}

void ServerManager::done(Connection *conn)
{
    if (!conn || !_nets || conn->_net_idx >= _net_worker_num) return ;
    _nets[conn->_net_idx].done(conn);
}
