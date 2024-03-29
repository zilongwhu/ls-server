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

typedef int (*LS_SRV_CALLBACK_FUN)(ls_srv_t server);
typedef int (*LS_SRV_ON_PROCESS_FUN)(ls_srv_t server, const netresult_t *);
typedef int (*LS_SRV_ON_ACCEPT_FUN)(ls_srv_t server, int sock, void **user_args);
typedef int (*LS_SRV_ON_INIT_FUN)(ls_srv_t server, int sock, void *user_args);
typedef int (*LS_SRV_ON_CLOSE_FUN)(ls_srv_t server, int sock, void *user_args);

ls_srv_t ls_srv_create(int size, LS_SRV_ON_PROCESS_FUN on_process, LS_SRV_ON_ACCEPT_FUN on_accept, LS_SRV_ON_INIT_FUN on_init, LS_SRV_ON_CLOSE_FUN on_close);

void ls_srv_set_callback(ls_srv_t server, LS_SRV_CALLBACK_FUN callback);

void ls_srv_set_userarg(ls_srv_t server, void *userarg);
void *ls_srv_get_userarg(ls_srv_t server);
int ls_srv_set_idle_timeout(ls_srv_t server, int timeout);

int ls_srv_listen(ls_srv_t server, const struct sockaddr *addr, socklen_t addrlen, int backlog);
int ls_srv_set_listen_fd(ls_srv_t server, int listen_fd);

int ls_srv_enable_notify(ls_srv_t server, int sock_fd);
int ls_srv_disable_notify(ls_srv_t server, int sock_fd);
void ls_srv_close_sock(ls_srv_t server, int sock_fd);

int ls_srv_read(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms);
int ls_srv_read_any(ls_srv_t server, int sock_fd, void *buf, size_t size, void *user_arg, int ms);
int ls_srv_write(ls_srv_t server, int sock_fd, const void *buf, size_t size, void *user_arg, int ms);

void ls_srv_run(ls_srv_t server);
void ls_srv_stop(ls_srv_t server);
void ls_srv_close(ls_srv_t server);

#endif
