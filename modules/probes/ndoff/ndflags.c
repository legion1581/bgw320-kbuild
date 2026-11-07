// Print the Broadcom netdev ext flags (bcm_nd_ext.iff_flags: bit0 WANDEV, bit5 WLANDEV, bit8 WLANDEV_DHD...) of all netdevs.
#include <linux/module.h>
#include <linux/netdevice.h>
static int __init ndflags_init(void)
{
	struct net_device *d;
	rcu_read_lock();
	for_each_netdev_rcu(&init_net, d)
		pr_info("ndflags: %-16s iff_flags 0x%05x%s\n", d->name, d->bcm_nd_ext.iff_flags,
			(d->bcm_nd_ext.iff_flags & BCM_IFF_WANDEV) ? "  WANDEV" : "");
	rcu_read_unlock();
	return -ENODEV;
}
module_init(ndflags_init);
MODULE_LICENSE("GPL");
