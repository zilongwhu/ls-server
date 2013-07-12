/*
 * =====================================================================================
 *
 *       Filename:  client2.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年06月25日 20时57分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAGIC_NUM   (0x11190309)

typedef struct net_head_s
{
    char _service[8];
    uint32_t _magic_num;
    uint32_t _body_len;
    uint32_t _reserved;
} net_head_t;

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7654);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    connect(sock, (struct sockaddr *)&addr, sizeof addr);
    char buf[] = "ABcdefG HI JK lmN";
    net_head_t reqhead, reshead;
    reqhead._magic_num = MAGIC_NUM;
    int len = sizeof buf;
    reqhead._body_len = len;
    write(sock, &reqhead, sizeof reqhead);
    write(sock, buf, len - 10);
    sleep(1);
    write(sock, buf + len - 10, 10);
    read(sock, &reshead, sizeof reshead);
    if (reshead._body_len != len)
    {
        return -1;
    }
    read(sock, buf, len);
    printf("[%s]\n", buf);
    close(sock);

    return 0;
}
