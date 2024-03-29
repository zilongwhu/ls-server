/*
 * =====================================================================================
 *
 *       Filename:  main2.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年06月25日 20时26分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "log.h"
#include "server_manager.h"
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

class SimpleServer;

class SimpleConn: public Connection
{
    public:
        int on_peer_close()
        {
            WARNING("peer closing sock[%d].", _sock_fd);
            return -1;
        }

        void *get_request_buffer()
        {
            if (_req_head._body_len > 1024)
            {
                WARNING("too long body len[%u]", _req_head._body_len);
                return NULL;
            }
            return _req_buf;
        }
        void *get_response_buffer()
        {
            return _res_buf;
        }

        int on_process()
        {
            for (unsigned int i = 0; i < _req_head._body_len; ++i)
            {
                _res_buf[i] = ::tolower(_req_buf[i]);
            }
            _res_head._body_len = _req_head._body_len;
            NOTICE("process request from sock[%d] ok, len=[%u].", _sock_fd, _req_head._body_len);
            return 0;
        }
    private:
        char _req_buf[1024];
        char _res_buf[1024];
};

class SimpleServer: public ServerManager
{
    public:
        Connection *on_accept(int sock)
        {
            return new SimpleConn;
        }
        void free_conn(Connection *conn)
        {
            delete conn;
        }
};

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7654);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    SimpleServer ss;
    ss.set_idle_timeout(1000);
    ss.set_read_timeout(150);
    ss.init(atoi(argv[1]), atoi(argv[2]));
    if (ss.run((struct sockaddr *)&addr, sizeof addr, 5) == 0)
    {
        while (1) sleep(10);
    }
    return 0;
}
