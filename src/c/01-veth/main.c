// gcc ./src/c/01-veth/main.c && sudo ./a.out
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>

#define MY_NETLINK_DEBUG 1


const short IFLA_ROOT = IFLA_MAX + 1;

struct rtattr_nest
{
    struct rtattr attr;
    size_t data_len;
    void *data_ptr;

    struct rtattr_nest *first_child;
    struct rtattr_nest *last_child;
    struct rtattr_nest *next_sibling;

#ifdef MY_NETLINK_DEBUG
    char *debug_info;
#endif
};

#ifdef MY_NETLINK_DEBUG
// TODO 优化 debug 信息
void rtattr_nest_print_debug(struct rtattr_nest *attr, int level)
{
    for (int i = 0; i < level * 2; i++) {
        printf(" ");
    }
    int child_num = 0;
    struct rtattr_nest *c = attr->first_child;
    while (c != NULL)
    {

        child_num += 1;
        c = c->next_sibling;
    }
    printf("%s (is_root=%d, data_len=%d, child_num=%d)\n", attr->debug_info, attr->attr.rta_type == IFLA_ROOT, attr->data_len, child_num);
    c = attr->first_child;
    while (c != NULL)
    {
        rtattr_nest_print_debug(c, level + 1);
        c = c->next_sibling;
    }
}
#endif

struct rtattr_nest *new_rtattr_nest(unsigned short rta_type, void *data_ptr, size_t data_len)
{
    struct rtattr_nest *attr = malloc(sizeof(struct rtattr_nest));
    memset(attr, 0, sizeof(struct rtattr_nest));
    attr->attr.rta_type = rta_type;
    attr->data_len = data_len;
    attr->data_ptr = data_ptr;
    return attr;
}

struct rtattr_nest *rtattr_nest_add(struct rtattr_nest *attr, unsigned short rta_type, void *data_ptr, size_t data_len)
{
    struct rtattr_nest *new_attr = new_rtattr_nest(rta_type, data_ptr, data_len);
    if (attr->first_child == NULL) 
    {
        attr->first_child = new_attr;
        attr->last_child = new_attr;
    } 
    else
    {
        attr->last_child->next_sibling = new_attr;
        attr->last_child = new_attr;
    }
    return new_attr;
}

int rtattr_nest_serialize(struct rtattr_nest *attr, char *buf, int offset)
{
    int next_offset = offset;
    if (attr->attr.rta_type != IFLA_ROOT)
    {
        // 先跳过 len
        next_offset += sizeof(attr->attr.rta_len);
        // 序列化 type
        memcpy(buf + next_offset, &attr->attr.rta_type, sizeof(attr->attr.rta_type));
        next_offset += sizeof(attr->attr.rta_type);
        // 序列化 data
        memcpy(buf + next_offset, attr->data_ptr, attr->data_len);
        next_offset += attr->data_len;
    }
    // 序列化孩子，并记录孩子总长度
    int children_len = 0;
    struct rtattr_nest *c = attr->first_child;
    while (c != NULL)
    {
        int l = NLMSG_ALIGN(rtattr_nest_serialize(c, buf, next_offset + children_len)); // NLMSG_ALIGN表示，进行 4 字节对齐
        children_len += l;
        c = c->next_sibling;
    }

    if (attr->attr.rta_type == IFLA_ROOT)
        return children_len;
    // 计算当前节点总长度，并序列化
    unsigned short len = NLMSG_ALIGN(RTA_LENGTH(children_len + attr->data_len)); // 最后一个可以不用 4 字节对齐？
    memcpy(buf + offset, &len, sizeof(len));
    // 返回长度
    return len;
}

int main()
{
    int rtnetlink_sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    struct
    {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char attrbuf[512];
    } req; //  route netlink socket 请求结构体
    unsigned int mtu = 1000;
    int nextSeqNr = 0;
    char *veth = "veth";
    char *veth0 = "veth0";
    char *veth0peer = "veth0peer";

    // 结构体设置为 0
    memset(&req, 0, sizeof(req));

    // 设置 netlink message header
    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(req.ifm)); //  len 字段，表示 req 结构体的总长度。
    printf("%d\n", req.nh.nlmsg_len);
    req.nh.nlmsg_type = RTM_NEWLINK;    //  新建一个 Link
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK; // 该请求包含本操作的全部请求内容
    req.nh.nlmsg_seq = ++nextSeqNr;

    // 设置 interface infomartion messsage
    req.ifm.ifi_family = AF_UNSPEC; // 未指定的地质族（netlink 固定填写此字段）
    req.ifm.ifi_index = 0; // interface （设备） index，创建时为 0
    req.ifm.ifi_flags = 0; // interface flag
    req.ifm.ifi_change = 0xffffffff; // interface flag 掩码

    // 设置 req.attrbuf

    // 创建根属性
    struct rtattr_nest *root = new_rtattr_nest(IFLA_ROOT, NULL, 0);
    // 设置名字
    struct rtattr_nest *name = rtattr_nest_add(root, IFLA_IFNAME, veth0, strlen(veth0));
    // 设置 link info
    struct rtattr_nest *linkInfo = rtattr_nest_add(root, IFLA_LINKINFO, NULL, 0);
    // 设置 link 的类型
    struct rtattr_nest *linkKind = rtattr_nest_add(linkInfo, IFLA_INFO_KIND, veth, strlen(veth));
    // 设置 link 的数据
    struct rtattr_nest *data = rtattr_nest_add(linkInfo, IFLA_INFO_DATA, NULL, 0);
    // 设置 link 的 peer
    struct ifinfomsg peer_ifm;
    memset(&peer_ifm, 0, sizeof(peer_ifm));
    peer_ifm.ifi_family = AF_UNSPEC;
    struct rtattr_nest *peer = rtattr_nest_add(data, VETH_INFO_PEER, &peer_ifm, sizeof(peer_ifm));
    struct rtattr_nest *peerName = rtattr_nest_add(peer, IFLA_IFNAME, veth0peer, strlen(veth0peer));

    // 序列化属性
    int attrs_len = rtattr_nest_serialize(root, req.attrbuf, 0);

    // 更新总长度
    req.nh.nlmsg_len = NLMSG_ALIGN(attrs_len + req.nh.nlmsg_len);

    printf("%d\n", attrs_len);
    printf("%d\n", req.nh.nlmsg_len);
#ifdef MY_NETLINK_DEBUG
    root->debug_info = "root";
    name->debug_info = "IFLA_IFNAME veth0";
    linkInfo->debug_info = "IFLA_LINKINFO";
    linkKind->debug_info = "IFLA_INFO_KIND veth";
    data->debug_info = "IFLA_INFO_DATA";
    peer->debug_info = "VETH_INFO_PEER data is ifinfomsg";
    peerName->debug_info = "IFLA_IFNAME veth0peer";
    rtattr_nest_print_debug(root, 0);
#endif

    send(rtnetlink_sock, &req, req.nh.nlmsg_len, 0);

    // TODO 完成接收消息并解析返回是否成功
}
