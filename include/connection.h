/*
 * =====================================================================================
 *
 *       Filename:  connection.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年07月20日 16时54分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#ifndef __LSNET_SERVER_CONNECTION_H__
#define __LSNET_SERVER_CONNECTION_H__

#include <strings.h>
#include "dlist.h"
#include "nethead.h"

class Connection
{
    public:
        enum
        {
            ST_NOT_INIT = 0,
            ST_INITED,
            ST_WAITING_REQUEST,
            ST_READING_REQUEST_HEAD,
            ST_READING_REQUEST_BODY,
            ST_PROCESSING_REQUEST,
            ST_PROCESSING_REQUEST_OK,
            ST_PROCESSING_REQUEST_FAIL,
            ST_WRITING_RESPONSE_HEAD,
            ST_WRITING_RESPONSE_BODY,
        };
    public:
        Connection()
        {
            DLIST_INIT(&_list);
            this->clear();
        }
        virtual ~ Connection()
        {
            DLIST_REMOVE(&_list);
            this->clear();
        }

        void clear()
        {
            _sock_fd = -1;
            _status = ST_NOT_INIT;
            _idx = 0;
            _net_idx = 0;
            ::bzero(&_req_head, sizeof _req_head);
            ::bzero(&_res_head, sizeof _res_head);
            _req_head._magic_num = MAGIC_NUM;
            _res_head._magic_num = MAGIC_NUM;
        }

        virtual void *get_request_buffer() = 0;
        virtual void *get_response_buffer() = 0;

        virtual int on_init() { return 0; }
        virtual int on_process() = 0;
        virtual void on_timeout() { }
        virtual void on_idle() { }
        virtual void on_error() { }
        virtual int on_peer_close() { return -1; }
        virtual void on_close() { }
    public:
        int _sock_fd;
        int16_t _idx;
        int8_t _net_idx;
        int8_t _status;

        net_head_t _req_head;
        net_head_t _res_head;

        __dlist_t _list;
};

#endif
