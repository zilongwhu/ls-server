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
#include "csrv.h"
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

class SimpleServer;

class SimpleConn: public Connection
{
    public:
        SimpleConn() { }
        ~ SimpleConn() { }

        char *get_req_buf(unsigned int &len)
        {
            len = sizeof _req_buf;
            return _req_buf;
        }
        char *get_res_buf(unsigned int &len)
        {
            len = _res_head._body_len;
            return _res_buf;
        }
        friend class SimpleServer;
    private:
        char _req_buf[256];
        char _res_buf[256];
};

class SimpleServer: public Server
{
    public:
        Connection *on_accept(int sock)
        {
            return new SimpleConn;
        }
        int on_init(Connection *conn)
        {
            return 0;
        }
        int on_process(Connection *conn)
        {
            SimpleConn *sc = (SimpleConn *)conn;
            for (unsigned int i = 0; i < sc->_req_head._body_len; ++i)
            {
                sc->_res_buf[i] = ::tolower(sc->_req_buf[i]);
            }
            sc->_res_head._body_len = sc->_req_head._body_len;
            return 0;
        }
        int on_timeout(Connection *conn)
        {
            return -1;
        }
        int on_idle(Connection *conn)
        {
            return -1;
        }
        int on_error(Connection *conn)
        {
            return -1;
        }
        int on_close(Connection *conn)
        {
            delete conn;
            return 0;
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
    ss.listen((struct sockaddr *)&addr, sizeof addr, 5);
    ss.run();

    return 0;
}
