// `
// 修改自：https://segmentfault.com/a/1190000009249039
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <linux/if_tun.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

const char *tun_name = "tun-sample";
const char *tun_ip = "172.16.2.1";
const char *tun_net_mask = "255.255.0.0";

int set_tun_if(char *if_name)
{
    // 简单起见，使用传统的 ioctl 系统调用，而非 netlink api。
    int sockfd, err;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        return 0;

    memset(&ifr, 0, sizeof ifr);
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

    // 设置 ip
    ifr.ifr_addr.sa_family = AF_INET;
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    inet_pton(AF_INET, tun_ip, &addr->sin_addr);
    if (err = ioctl(sockfd, SIOCSIFADDR, &ifr) < 0)
        return err;
    // 设置网络掩码
    ifr.ifr_netmask.sa_family = AF_INET;
    addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    inet_pton(AF_INET, tun_net_mask, &addr->sin_addr);
    if (err = ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0)
        return err;

    // 启动接口
    ifr.ifr_flags |= IFF_UP;
    if (err = ioctl(sockfd, SIOCSIFFLAGS, &ifr) <0)
        return err;
    close(sockfd);
    return 0;
}


int tun_alloc(int flags)
{

    // 没有找到如何使用 netlink 创建 tun 设备的相关示例。
    // https://man7.org/linux/man-pages/man7/netdevice.7.html
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    // 打开 /dev/net/tun 文件，即创建一个用于收发 tun 虚拟网络设备的文件描述符
    // 该文件一般是 o666 权限，因此不需要特殊权限。
    if ((fd = open(clonedev, O_RDWR)) < 0)
    {
        return fd;
    }

    // 设置 tun 虚拟网络设备
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;  // 设置设备标志
    strncpy(ifr.ifr_name, tun_name, IFNAMSIZ); // 设置设备名

    // 如果该 tun 设备不存在，内核创建一个 tun 设备（通过： ip addr show 可以看到），将文件描述符和虚拟网络设备关联。该进程退出后，该设备将自动被删除。
    // 如果该 tun 设备已经存在，则仅仅将文件描述符和虚拟网络设备关联。该进程退出后，设备仍然存在。
    // 该系统调用需要 CAP_NET_ADMIN 权限。
    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
    {
        close(fd);
        return err;
    }

    printf("Open tun/tap device: %s for reading...\n", ifr.ifr_name);

    // 设置 ip、网络掩码 并 启动 tun 设备
    // 等价于执行：
    //   sudo ip addr add 172.16.2.1/8 dev tun-sample
    //   sudo ip link set tun-sample up
    // 会自动添加路由： 172.16.0.0/16 dev tun-sample proto kernel scope link src 172.16.2.1 （ip route show）
    if ((err = set_tun_if(ifr.ifr_name)) < 0)
    {
        close(fd);
        return err;
    }
    return fd;
}

int main()
{
    int tun_fd, nread;
    char buffer[1500];

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers) 即 IP 包
     *        IFF_TAP   - TAP device 以太网包（包含 Ethernet headers）
     *        IFF_NO_PI - Do not provide packet information，不包含额外的报信息，即传递到 tun_fd 中数据是纯粹的 ip 包。
     *                    如果不设置该选项，传递到 tun_fd 中数据将包含 struct tun_pi { unsigned short flags; unsigned short proto; }
     *                              flags - 设置 TUN_PKT_STRIP 选项时，表示用户缓冲区大小
     *                              proto - 表示当前 IP 包的协议，https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
     */
    tun_fd = tun_alloc(IFF_TUN | IFF_NO_PI);

    if (tun_fd < 0)
    {
        perror("Allocating interface");
        exit(1);
    }

    // 该程序接收 tun 数据包后，仅打印收到的包长度，不做任何事情。
    while (1)
    {
        nread = read(tun_fd, buffer, sizeof(buffer));
        if (nread < 0)
        {
            perror("Reading from interface");
            close(tun_fd);
            exit(1);
        }

        printf("Read %d bytes from tun/tap device\n", nread);
    }
    return 0;
}