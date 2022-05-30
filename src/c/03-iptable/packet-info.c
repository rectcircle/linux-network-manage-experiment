// 待完善
// 必须安装 iptables 否则会报错：getsockopt error: Protocol not available
// sudo iptables -t nat -A PREROUTING -p tcp --dport 12345 -j REDIRECT --to-ports 1234
// sudo iptables -t nat -I OUTPUT -p tcp -o lo --dport 12345 -j REDIRECT --to-ports 1234 # 支持本地回环
// gcc ./src/c/03-iptable/packet-info.c && sudo ./a.out
// nc localhost 1234
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// https://elixir.bootlin.com/linux/v5.15.3/source/include/uapi/linux/netfilter_ipv4.h#L52
#include<linux/netfilter_ipv4.h>

#define BUFFER_SIZE 1024
#define BACKLOG 5

int main(int argc, char *argv[])
{
    int sfd = 0;
    int cfd = 0;
    int n = 0;
    int port = 1234;
    struct sockaddr_in server_addr;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
    {
        perror("socket error");
        exit(-1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    n = bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (n < 0)
    {
        perror("bind error");
        exit(-1);
    }
    n = listen(sfd, BACKLOG);
    if (n < 0)
    {
        perror("listen error");
        exit(-1);
    }
    struct sockaddr_in source_addr;
    memset(&source_addr, 0, sizeof(source_addr));
    socklen_t client_addr_len;
    client_addr_len = sizeof(source_addr);

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    socklen_t dest_addr_len;
    dest_addr_len = sizeof(dest_addr);

    struct sockaddr_in original_dest_addr;
    memset(&original_dest_addr, 0, sizeof(original_dest_addr));
    socklen_t original_dest_addr_len;
    original_dest_addr_len = sizeof(original_dest_addr);

    while (1)
    {
        cfd = accept(sfd, (struct sockaddr *)&source_addr, &client_addr_len);
        if (cfd < 0)
        {
            perror("accept error!");
            exit(-1);
        }

        // 获取到的是接收到的包上的 dest ip 和 port
        n = getsockname(cfd, (struct sockaddr *)&dest_addr, &dest_addr_len);
        if (n < 0)
        {
            perror("getsockname error");
            exit(-1);
        }

        // 获取到的是 nat 之前的原始的 dest ip 和 port
        n = getsockopt(cfd, SOL_IP, SO_ORIGINAL_DST, &original_dest_addr, &original_dest_addr_len);
        if (n < 0) 
        {
            perror("getsockopt error");
            exit(-1);
        }

        printf("source{ip: %s, port: %d};  dest{ip: %s, port: %d}; original dest{ip: %s, port: %d}\n", 
            inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port), 
            inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port),
            inet_ntoa(original_dest_addr.sin_addr), ntohs(original_dest_addr.sin_port)
        );

        char send_buff[BUFFER_SIZE];
        memset(send_buff, 0, sizeof(send_buff));
        sprintf(send_buff, "source{ip: %s, port: %d};  dest{ip: %s, port: %d}; original dest{ip: %s, port: %d}\n",
            inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port),
            inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port),
            inet_ntoa(original_dest_addr.sin_addr), ntohs(original_dest_addr.sin_port)
        );
        send(cfd, send_buff, strlen(send_buff), 0);
        close(cfd);
    }
}
