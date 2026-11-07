// nfct_nsfix: let network namespaces be created on the stock BGW320 5.15.167 kernel.
//
// Broadcom added ~17 sysctls to nf_conntrack (nf_conntrack_bbm_mode, _cgnat_mode, _public_ip_address,
// _max_exceeded, the *_ports limits) whose `data` points at module globals, mode 0644, and never marked
// them read-only for child namespaces (upstream does that for nf_conntrack_max/_buckets/_expect_max in
// nf_conntrack_standalone_init_sysctl). On the first `unshare(CLONE_NEWNET)` register_net_sysctl()'s
// ensure_safe_net_sysctl() WARNs once per such entry. Harmless upstream - but Broadcom's __warn()
// writes a crash file (kpanic_write_file: filp_open/kernel_write) from inside the brk exception handler,
// where preemption is disabled -> "BUG: scheduling while atomic" cascade -> watchdog reset.
//
// Fix: clear the write bits of those entries in nf_conntrack's static template `nf_ct_sysctl_table`
// (static, not exported: its address comes from /proc/kallsyms via the `tab` parameter). The init_net
// table was kmemdup'ed from the template at module load and stays writable; every namespace created
// after this module is loaded gets a read-only copy, exactly what upstream does for its own globals.
// Stock userspace never writes these sysctls (checked in the stock squashfs).
//
// Build: ./build-module.sh modules/<dir> (see README.md). Load: insmod nfct_nsfix.ko tab=0x$(awk '$3=="nf_ct_sysctl_table"{print $1}' /proc/kallsyms)
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>

static unsigned long tab;
module_param(tab, ulong, 0444);
MODULE_PARM_DESC(tab, "address of nf_conntrack's nf_ct_sysctl_table (from /proc/kallsyms)");
static int dryrun;
module_param(dryrun, int, 0444);
MODULE_PARM_DESC(dryrun, "1 = only report what would change");

// entries with writable mode whose data is a kernel/module global in the stock nf_conntrack.ko
static const char * const names[] = {
	"nf_conntrack_max", "nf_conntrack_buckets", "nf_conntrack_expect_max",
	"nf_conntrack_max_other_ports_allowed", "nf_conntrack_max_other_user_ports",
	"nf_conntrack_max_other_internal_ports", "nf_conntrack_min_other_reserved_ports",
	"nf_conntrack_cgnat_mode", "nf_conntrack_public_ip_address", "nf_conntrack_max_exceeded",
	"nf_conntrack_bbm_mode",
	"nf_conntrack_max_tcp_ports_allowed", "nf_conntrack_max_tcp_user_ports",
	"nf_conntrack_max_tcp_internal_ports", "nf_conntrack_min_tcp_reserved_ports",
	"nf_conntrack_max_udp_ports_allowed", "nf_conntrack_max_udp_user_ports",
	"nf_conntrack_max_udp_internal_ports", "nf_conntrack_min_udp_reserved_ports",
};
#define MAXENT 128

static struct ctl_table *patched[MAXENT];
static umode_t saved[MAXENT];
static int npatched;

static bool listed(const char *n)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(names); i++)
		if (!strcmp(n, names[i]))
			return true;
	return false;
}

static int __init nsfix_init(void)
{
	struct ctl_table *t = (struct ctl_table *)tab, ent;
	char name[64];
	int i;

	if (!tab)
		return -EINVAL;
	for (i = 0; i < MAXENT; i++) {
		if (copy_from_kernel_nofault(&ent, &t[i], sizeof(ent))) {
			pr_err("nfct_nsfix: cannot read entry %d at %px\n", i, &t[i]);
			goto bad;
		}
		if (!ent.procname)
			break;
		if (copy_from_kernel_nofault(name, ent.procname, sizeof(name))) {
			pr_err("nfct_nsfix: cannot read procname of entry %d\n", i);
			goto bad;
		}
		name[sizeof(name) - 1] = 0;
		if (i == 0 && strcmp(name, "nf_conntrack_max")) {
			pr_err("nfct_nsfix: entry 0 is \"%s\", not nf_conntrack_max - wrong address\n", name);
			return -EINVAL;
		}
		if (!(ent.mode & 0222) || !ent.data || !listed(name))
			continue;
		pr_info("nfct_nsfix: %-40s 0%o -> 0%o (data %px)%s\n", name, ent.mode, ent.mode & ~0222,
			ent.data, dryrun ? " [dry run]" : "");
		if (dryrun)
			continue;
		patched[npatched] = &t[i];
		saved[npatched] = ent.mode;
		npatched++;
		t[i].mode = ent.mode & ~0222;
	}
	if (i < 40 || i >= MAXENT) {
		pr_err("nfct_nsfix: %d entries, does not look like nf_ct_sysctl_table\n", i);
		goto bad;
	}
	pr_info("nfct_nsfix: %d entries, %d made read-only for new network namespaces\n", i, npatched);
	return 0;
bad:
	while (npatched--)
		patched[npatched]->mode = saved[npatched];
	npatched = 0;
	return -EINVAL;
}

static void __exit nsfix_exit(void)
{
	while (npatched--)
		patched[npatched]->mode = saved[npatched];
}

module_init(nsfix_init);
module_exit(nsfix_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BGW320: read-only Broadcom nf_conntrack sysctls in child network namespaces");
