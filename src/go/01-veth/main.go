package main

// sudo go ./src/go/01-veth/

import (
	"fmt"
	"net"
	"os"
	"os/exec"

	sysctl "github.com/lorenzosaino/go-sysctl"
	"github.com/vishvananda/netlink"
)

const (
	beforeScript = "echo '===初始状态网络设备' && ip addr show && echo" +
		" && echo '===初始状态 arp 表' && cat /proc/net/arp && echo"
	afterScript = "echo '===配置完 veth 后网络设备' && ip addr show && echo" +
		" && echo '===尝试是否可以 ping 通' && ping -c 4 192.168.4.3 -I veth0 && echo" +
		" && echo '===ping 完成后 arp 表' && cat /proc/net/arp && echo " +
		" && sudo ip link delete veth0"
)

func runtScript(script string) error {
	cmd := exec.Command("/bin/sh", "-c", script)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func panicIfErr(err error) {
	if err != nil {
		panic(err)
	}
}

func main() {
	// 输出初始化状态
	panicIfErr(runtScript(beforeScript))

	fmt.Println("===创建并配置veth")
	// 创建一对 veth
	panicIfErr(netlink.LinkAdd(&netlink.Veth{
		LinkAttrs: netlink.LinkAttrs{
			Name: "veth0",
		},
		PeerName: "veth0peer",
	}))
	// 配置 ip 地址
	ip, ipNet, err := net.ParseCIDR("192.168.4.2/24")
	ipNet.IP = ip
	if err != nil {
		panic(err)
	}
	panicIfErr(netlink.AddrAdd(netlink.NewLinkBond(netlink.LinkAttrs{Name: "veth0"}), &netlink.Addr{IPNet: ipNet}))

	ip, ipNet, err = net.ParseCIDR("192.168.4.3/24")
	ipNet.IP = ip
	if err != nil {
		panic(err)
	}
	panicIfErr(netlink.AddrAdd(netlink.NewLinkBond(netlink.LinkAttrs{Name: "veth0peer"}), &netlink.Addr{IPNet: ipNet}))

	netlink.LinkSetUp(netlink.NewLinkBond(netlink.LinkAttrs{Name: "veth0"}))
	netlink.LinkSetUp(netlink.NewLinkBond(netlink.LinkAttrs{Name: "veth0peer"}))

	panicIfErr(sysctl.Set(fmt.Sprintf("net.ipv4.conf.%s.accept_local", "veth0"), "1"))
	panicIfErr(sysctl.Set(fmt.Sprintf("net.ipv4.conf.%s.accept_local", "veth0peer"), "1"))
	fmt.Println("完成创建并配置veth")
	fmt.Println()

	// 实验
	panicIfErr(runtScript(afterScript))
}
