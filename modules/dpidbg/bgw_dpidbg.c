// bgw_dpidbg: log the blog flow events exactly as dpiqos.ko sees them - the BlogFlowEventInfo_t fields and, per
// conntrack entry, the DPI flags / app id / queue - to find out why dpiqos puts classified connections into bypass.
// Test-only module for the bench; built on the reconstructed tree (make-ack-tree.sh) like bgw_warnfix. Output: dmesg, tag dpidbg.
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/blog.h>
#include <linux/dpi.h>
#include <linux/netdevice.h>

/* struct nf_conn's Broadcom extension is not in the ack13 headers: dpiqos.ko reads the dpi_info block at ct+312 */
#define CT_DPI_INFO_OFF 312

static int dpidbg_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	BlogFlowEventInfo_t *info = ptr;
	int i;

	if (!info)
		return NOTIFY_DONE;
	pr_info("dpidbg: event %lu ct_count %d u32 0x%08x: ds %u type %u us %u tr471 %u rxch %u txch %u flow_id 0x%02x\n",
		event, info->ct_count, info->u32, info->is_downstream, info->flow_event_type, info->is_upstream,
		info->is_tr471_flow, info->rx_channel, info->tx_channel, info->skb_mark_flow_id);
	for (i = 0; i < info->ct_count && i < BLOG_CT_MAX; i++) {
		void *ct = info->ct_p[i];
		struct dpi_info *di;
		if (!ct)
			continue;
		di = (struct dpi_info *)((char *)ct + CT_DPI_INFO_OFF);
		pr_info("dpidbg:   ct[%d] %p: dpi flags 0x%lx app %u dev %p appinst %p eg_prio %u net_dev %s\n",
			i, ct, di->flags, di->app ? dpi_app_id(di->app) : 0, di->dev, di->appinst, di->eg_prio,
			di->net_dev ? di->net_dev->name : "-");
	}
	return NOTIFY_DONE;
}

static struct notifier_block dpidbg_nb = { .notifier_call = dpidbg_event };

static int __init dpidbg_init(void)
{
	int rc = blog_flowevent_register_notifier(&dpidbg_nb);
	pr_info("dpidbg: registered (%d)\n", rc);
	return rc;
}

static void __exit dpidbg_exit(void)
{
	blog_flowevent_unregister_notifier(&dpidbg_nb);
	pr_info("dpidbg: unregistered\n");
}

module_init(dpidbg_init);
module_exit(dpidbg_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BGW320 bench: log blog flow events with DPI state");
