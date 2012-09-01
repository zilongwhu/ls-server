/*
 * =====================================================================================
 *
 *       Filename:  server.h
 *
 *    Description:  lsnet server framework interface
 *
 *        Version:  1.0
 *        Created:  2012年05月25日 10时35分37秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#ifndef __LS_SERVER_H__
#define __LS_SERVER_H__

#include <sys/socket.h>
#include "exnet.h"

typedef void *ls_srv_t;

typedef int (*LS_SRV_CALLBACK_FUN)(ls_srv_t, const netresult_t *);
typedef int (*LS_SRV_ON_ACCEPT_FUN)(ls_srv_t, int sock);

ls_srv_t ls_srv_create(int size, LS_SRV_CALLBACK_FUN proc, LS_SRV_ON_ACCEPT_FUN on_accept);
void ls_srv_close(ls_srv_t server);
void ls_srv_stop(ls_srv_t server);
int ls_srv_listen(ls_srv_t server, const struct sockaddr *addr, socklen_t addrlen, int backlog);
int ls_srv_set_listen_fd(ls_srv_t server, int listen_fd);
int ls_srv_set_idle_timeout(ls_srv_t server, int timeout);
int ls_srv_read(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms);
int ls_srv_read_any(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms);
int ls_srv_write(ls_srv_t server, int sock_fd, const void *buf, size_t size, void *user_arg, int ms);
void ls_srv_run(ls_srv_t server);

#endif
