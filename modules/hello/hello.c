// hello: the smallest useful test that the build tree matches the stock BGW320 kernel.
// If the tree is wrong, insmod refuses it (vermagic) or the kernel calls a wrong pointer as init (struct module).
// Build: ./build-module.sh modules/hello    Load on the router: insmod hello.ko && dmesg | tail -2 && rmmod hello
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

static int __init hello_init(void)
{
	// the two Broadcom-grown structs: stock values are 2240 / 576 (upstream 5.15.167 with the stock config: 1984 / 224)
	pr_info("hello: BGW320 module loaded, sizeof(net_device)=%zu sizeof(sk_buff)=%zu\n",
		sizeof(struct net_device), sizeof(struct sk_buff));
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello: unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Build-tree sanity check for the AT&T BGW320 stock kernel");
