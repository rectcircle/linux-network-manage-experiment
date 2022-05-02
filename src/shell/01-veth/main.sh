#!/usr/bin/env bash
# sudo ./src/shell/01-veth/main.sh 

# 观察网卡情况
echo '===初始状态网络设备'
ip addr show
echo

echo '===初始状态 arp 表'
cat /proc/net/arp
echo

echo '===创建并配置veth'
# 创建一对 veth
sudo ip link add veth0 type veth peer name veth0peer
# 给这一对 veth 配置 ip 地址
sudo ip addr add 192.168.4.2/24 dev veth0
sudo ip addr add 192.168.4.3/24 dev veth0peer
# 启动这两个网卡
sudo ip link set veth0 up
sudo ip link set veth0peer up
# 允许从非 lo 设备进来的数据包的源 IP 地址是本机地址
sudo sysctl -w net.ipv4.conf.veth0.accept_local=1
sudo sysctl -w net.ipv4.conf.veth0peer.accept_local=1
echo '完成创建并配置veth'
echo

# 观察 arp
echo '===配置完 veth 后网络设备'
ip addr show
echo

# 实验
echo '===尝试是否可以 ping 通'
ping -c 4 192.168.4.3 -I veth0
echo

echo '===ping 完成后 arp 表'
cat /proc/net/arp
echo

# 恢复现场
sudo ip link delete veth0