// Print struct net_device field offsets of the stock-layout ACK tree (which field sits at 0x214?).
#include <linux/module.h>
#include <linux/netdevice.h>
#define P(f) pr_info("ndoff: %-28s 0x%03zx size %zu\n", #f, offsetof(struct net_device, f), sizeof(((struct net_device *)0)->f))
static int __init ndoff_init(void)
{
	P(flags); P(priv_flags); P(gflags); P(features); P(mtu); P(type);
	P(bcm_nd_ext); P(stats); P(rx_dropped); P(ifindex); P(group);
	return -ENODEV;   /* print and do not stay loaded */
}
module_init(ndoff_init);
MODULE_LICENSE("GPL");
