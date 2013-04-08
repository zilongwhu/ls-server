/*
 * =====================================================================================
 *
 *       Filename:  server.c
 *
 *    Description:  lsnet server framework
 *
 *        Version:  1.0
 *        Created:  2012年05月25日 10时34分04秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#include <errno.h>
#include <stdlib.h>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "server.h"

enum
{
	SERVER_NOT_INIT,
	SERVER_SHARED = 0x01,
	SERVER_NOT_SHARED = 0x02,
	SERVER_STOP = 0x04,
};

struct ls_server
{
	int _listen_fd;
	uint32_t _status;
	int _idle_timeout;
	epex_t _epoll;
	LS_SRV_CALLBACK_FUN _proc;
	LS_SRV_ON_ACCEPT_FUN _on_accept;
	LS_SRV_ON_CLOSE_FUN _on_close;
	netresult_t _results[256];
};

ls_srv_t ls_srv_create(int size, LS_SRV_CALLBACK_FUN proc, LS_SRV_ON_ACCEPT_FUN on_accept, LS_SRV_ON_CLOSE_FUN on_close)
{
	if ( size <= 0 || NULL == proc || NULL == on_accept || NULL == on_close )
	{
		WARNING("invalid args: size[%d], proc[%p], on accept[%p], on close[%p].", size, proc, on_accept, on_close);
		return NULL;
	}
	struct ls_server *srv = (struct ls_server *)calloc(1, sizeof(struct ls_server));
	if ( NULL == srv )
	{
		WARNING("failed to alloc server.");
		return NULL;
	}
	srv->_epoll = epex_open(size);
	if ( NULL == srv->_epoll )
	{
		WARNING("failed to init epex.");
		free(srv);
		return NULL;
	}
	srv->_listen_fd = -1;
	srv->_status = SERVER_NOT_INIT;
	srv->_idle_timeout = -1;
	srv->_proc = proc;
	srv->_on_accept = on_accept;
	srv->_on_close = on_close;
	return srv;
}

void ls_srv_close(ls_srv_t server)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return ;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_SHARED & srv->_status )
	{
		epex_close(srv->_epoll);
	}
	else if ( SERVER_NOT_SHARED & srv->_status )
	{
		epex_close(srv->_epoll);
		SAFE_CLOSE(srv->_listen_fd);
	}
	free(server);
}

void ls_srv_stop(ls_srv_t server)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return ;
	}
	struct ls_server *srv = (struct ls_server *)server;
	srv->_status |= SERVER_STOP;
}

int ls_srv_listen(ls_srv_t server, const struct sockaddr *addr, socklen_t addrlen, int backlog)
{
	if ( NULL == server || NULL == addr || addrlen <= 0 || backlog <= 0 )
	{
		WARNING("invalid args: server[%p], addr[%p], addrlen[%d], backlog[%d].", server, addr, (int)addrlen, backlog);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT != srv->_status )
	{
		WARNING("server already has listen fd.");
		return -1;
	}
	srv->_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if ( srv->_listen_fd < 0 )
	{
		WARNING("failed to create socket, error[%d].", errno);
		return -1;
	}
	if ( bind(srv->_listen_fd, addr, addrlen) < 0 )
	{
		WARNING("failed to bind socket, error[%d].", errno);
		goto FAIL;
	}
	if ( !epex_listen2(srv->_epoll, srv->_listen_fd, backlog, NULL) )
	{
		WARNING("failed to attach listen fd to epex.");
		goto FAIL;
	}
	srv->_status = SERVER_NOT_SHARED;
	return 0;
FAIL:
	SAFE_CLOSE(srv->_listen_fd);
	srv->_listen_fd = -1;
	return -1;
}

int ls_srv_set_listen_fd(ls_srv_t server, int listen_fd)
{
	if ( NULL == server || listen_fd < 0 )
	{
		WARNING("invalid args: server[%p], listen fd[%d].", server, listen_fd);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT != srv->_status )
	{
		WARNING("server already has listen fd.");
		return -1;
	}
	if ( !epex_listen(srv->_epoll, listen_fd, NULL) )
	{
		WARNING("failed to attach listen fd to epex.");
		return -1;
	}
	srv->_listen_fd = listen_fd;
	srv->_status = SERVER_SHARED;
	return 0;
}

int ls_srv_set_idle_timeout(ls_srv_t server, int timeout)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( timeout <= 0 )
	{
		timeout = -1;
	}
	srv->_idle_timeout = timeout;
	return 0;
}

int ls_srv_read(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT == srv->_status )
	{
		WARNING("server is not init.");
		return -1;
	}
	if ( !epex_read(srv->_epoll, sock_fd, buf, size, user_arg, ms) )
	{
		WARNING("epex_read return false.");
		return -1;
	}
	return 0;
}

int ls_srv_read_any(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT == srv->_status )
	{
		WARNING("server is not init.");
		return -1;
	}
	if ( !epex_read_any(srv->_epoll, sock_fd, buf, size, user_arg, ms) )
	{
		WARNING("epex_read return false.");
		return -1;
	}
	return 0;
}

int ls_srv_write(ls_srv_t server, int sock_fd, const void *buf, size_t size, void *user_arg, int ms)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return -1;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT == srv->_status )
	{
		WARNING("server is not init.");
		return -1;
	}
	if ( !epex_write(srv->_epoll, sock_fd, buf, size, user_arg, ms) )
	{
		WARNING("epex_write return false.");
		return -1;
	}
	return 0;
}

void ls_srv_run(ls_srv_t server)
{
	if ( NULL == server )
	{
		WARNING("invalid args: server[%p].", server);
		return ;
	}
	struct ls_server *srv = (struct ls_server *)server;
	if ( SERVER_NOT_INIT == srv->_status )
	{
		WARNING("server is not init.");
		return ;
	}
	int i;
	int cnt;
	int ret;
	int sock;
	int listen_ok = 1;
	netresult_t result;
	do
	{
		cnt = epex_poll(srv->_epoll, srv->_results, 256);
		for ( i = 0; i < cnt; ++i )
		{
			if ( srv->_listen_fd == srv->_results[i]._sock_fd )
			{
				switch (srv->_results[i]._op_type)
				{
					case NET_OP_ACCEPT:
						do
						{
							sock = accept(srv->_listen_fd, NULL, NULL);
							if ( sock >= 0 )
							{
                                void *user_args = NULL;
								if ( srv->_on_accept(srv, sock, &user_args) < 0 )
								{
									WARNING("on accept failed, close sock[%d].", sock);
									SAFE_CLOSE(sock);
								}
                                else if ( !epex_attach(srv->_epoll, sock, user_args, srv->_idle_timeout) )
								{
									WARNING("failed to attach sock to epex.");
                                    srv->_on_close(srv, sock, user_args);
									SAFE_CLOSE(sock);
								}
								else
								{
									DEBUG("accept sock[%d] ok.", sock);
								}
							}
						} while (sock >= 0);
						break;
					default:
						FATAL("listen fd, unexpected op type[%hu], should not run to here.", srv->_results[i]._op_type);
                        epex_detach(srv->_epoll, srv->_listen_fd, NULL);
                        if (SERVER_NOT_SHARED == srv->_status)
                        {
                            SAFE_CLOSE(srv->_listen_fd);
                        }
                        else
                        {
                            srv->_on_close(srv, srv->_listen_fd, NULL);
                        }
                        srv->_listen_fd = -1;
						break;
				}
			}
			else
			{
				result = srv->_results[i];
				ret = srv->_proc(srv, &result);
                if ( NET_OP_NOTIFY == result._op_type || ret < 0 )
                {
                    if ( NET_ERROR == result._status )
                    {
                        WARNING("sock[%d] has error[%s].", result._sock_fd, strerror_t(result._errno));
                    }
                    else if ( NET_EIDLE == result._status )
                    {
                        WARNING("sock[%d] enters idle.", result._sock_fd);
                    }
					DEBUG("exec close sock[%d].", result._sock_fd);
                    void *user_args = NULL;
					epex_detach(srv->_epoll, srv->_results[i]._sock_fd, &user_args);
                    srv->_on_close(srv, sock, user_args);
					SAFE_CLOSE(srv->_results[i]._sock_fd);
				}
			}
		}
	} while( !(SERVER_STOP & srv->_status) && listen_ok );
}
