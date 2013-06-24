/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2013年04月08日 22时43分24秒
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
    int len = sizeof buf;
    write(sock, &len, sizeof len);
    write(sock, buf, len - 10);
//    sleep(1);
    write(sock, buf + len - 10, 10);
    read(sock, &len, sizeof len);
    read(sock, buf, len);
    printf("%s\n", buf);
    close(sock);

    return 0;
}
