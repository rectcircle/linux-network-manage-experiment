#!/usr/bin/env bash
# sudo ./src/shell/02-bridge/main.sh

# 创建两对 veth
sudo ip link add veth0 type veth peer name veth0peer
sudo ip link add veth1 type veth peer name veth1peer
# 给这两对 veth 的对侧配置 ip 地址
sudo ip addr add 192.168.57.4/24 dev veth0peer
sudo ip addr add 192.168.57.5/24 dev veth1peer
# 启动这两对网卡
sudo ip link set veth0 up
sudo ip link set veth1 up
sudo ip link set veth0peer up
sudo ip link set veth1peer up
# 允许从非 lo 设备进来的数据包的源 IP 地址是本机地址
sudo sysctl -w net.ipv4.conf.veth0.accept_local=1
sudo sysctl -w net.ipv4.conf.veth0peer.accept_local=1
sudo sysctl -w net.ipv4.conf.veth1.accept_local=1
sudo sysctl -w net.ipv4.conf.veth1peer.accept_local=1

# 删除物理网络接口 enp0s9 的 IP 地址
sudo ip addr del 192.168.57.3/24 dev enp0s9

# 创建设置并启动 br0
sudo ip link add name br0 type bridge
sudo ip link set dev veth0 master br0
sudo ip link set dev veth1 master br0
sudo ip link set dev enp0s9 master br0
sudo ip link set br0 up

### 验证不 bridge 配置 ip 情况
# 通过 veth0 ping veth0peer：不通
# ping -c 1 192.168.57.4 -I veth0 
# 通过 veth0peer ping veth1peer：第一次可以 ping 通，第二次无法 ping 通，时通时不通。
# ping -c 2 192.168.57.5 -I veth0peer
# 虚拟机外部 ping：时通时不通。
# ping 192.168.57.5

# 配置 br0 ip
sudo ip addr add 192.168.57.3/24 dev br0

### 验证 bridge 配置 ip 情况
# 通过 veth0 ping veth0peer：不通
# ping -c 1 192.168.57.4 -I veth0 
# 通过 veth0peer ping veth1peer：第一次可以 ping 通，第二次无法 ping 通，时通时不通。
# ping -c 2 192.168.57.5 -I veth0peer
# 虚拟机外部 ping：时通时不通。
# ping 192.168.57.5
# ping 192.168.57.3