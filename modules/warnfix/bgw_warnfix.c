// bgw_warnfix: make WARN() harmless on the stock BGW320 5.15.167 kernel.
//
// Broadcom's __warn() saves a crash report to flash: it calls kpanic_write_file() (filp_open / kernel_write /
// filp_close on /data/crashdump). Its guard skips the write only when preempt_count > 1, but a WARN() on arm64
// arrives through a BRK debug exception whose entry already disabled preemption once, so the guard passes and the
// file I/O sleeps in atomic context -> "BUG: scheduling while atomic" cascade -> watchdog reset. Any warning in
// process context (a driver, a sysctl table, a container workload...) therefore reboots the router.
//
// Fix: one instruction in __warn. At __warn+0x160 the `bl kpanic_write_file` becomes `b __warn+0x174`, which skips
// the call and its error print and lands on the code that clears the crash buffers and the crash-mode flag (the
// flag is shared with die()/panic()/the console capture, so it must still be cleared). Warnings then behave as on
// any Linux: message + backtrace in the log, the system keeps running. oops/panic crash files are untouched (they
// do not go through __warn).
//
// The patch goes through the kernel's own text patcher aarch64_insn_patch_text_nosync() (fixmap write + I-cache
// maintenance, as used for jump labels). BL <-> B is in the set of instructions the Arm ARM allows to be modified
// while other CPUs may execute them, so no stop_machine is needed. The patcher is not exported: its address is a
// module parameter taken from /proc/kallsyms, like __warn's. Everything is verified before writing: both symbol
// names, the surrounding instructions, the BL target; any mismatch -> refuse, nothing changed. rmmod restores.
//
// Build: ./build-module.sh modules/<dir> (see README.md). Load (/etc/init.d/bgw-warnfix):
//   insmod bgw_warnfix.ko warn=0x<__warn> patcher=0x<aarch64_insn_patch_text_nosync>   (from /proc/kallsyms)
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>

static unsigned long warn, patcher;
module_param(warn, ulong, 0444);
MODULE_PARM_DESC(warn, "address of __warn (from /proc/kallsyms)");
module_param(patcher, ulong, 0444);
MODULE_PARM_DESC(patcher, "address of aarch64_insn_patch_text_nosync (from /proc/kallsyms)");

#define CALL_OFF	0x160	/* bl kpanic_write_file */
#define RESUME_OFF	0x174	/* mov x2, #0x1400: memset of the crash buffers, then flag = 0 */

/* expected code around the call (stock kernel, dump/home_boot/home_kernel.elf) */
static const struct { unsigned int off; u32 insn; } ctx[] = {
	{ 0x150, 0xb94e0b40 },	/* ldr  w0, [x26, #3592]   crash-mode flag */
	{ 0x154, 0x7100081f },	/* cmp  w0, #0x2           2 = warning */
	{ 0x158, 0x54000201 },	/* b.ne __warn+0x198 */
	{ 0x15c, 0xaa1503e0 },	/* mov  x0, x21            crash buffer */
	{ 0x164, 0x34000080 },	/* cbz  w0, __warn+0x174 */
	{ RESUME_OFF, 0xd2828002 },	/* mov x2, #0x1400 */
};

typedef int (*patch_fn)(void *addr, u32 insn);
static u32 orig_insn;
static bool patched;

static int sym_is(unsigned long addr, const char *name)
{
	char buf[KSYM_SYMBOL_LEN];
	size_t n = strlen(name);

	sprint_symbol(buf, addr);
	return !strncmp(buf, name, n) && !strncmp(buf + n, "+0x0/", 5);
}

static int read_insn(unsigned long addr, u32 *insn)
{
	return copy_from_kernel_nofault(insn, (void *)addr, sizeof(*insn));
}

static u32 b_insn(unsigned long from, unsigned long to)
{
	return 0x14000000 | (((to - from) >> 2) & 0x03ffffff);
}

static int __init warnfix_init(void)
{
	unsigned long call = warn + CALL_OFF, target;
	u32 insn, new = b_insn(call, warn + RESUME_OFF);
	int i, ret;

	if (!warn || !patcher)
		return -EINVAL;
	if (!sym_is(warn, "__warn") || !sym_is(patcher, "aarch64_insn_patch_text_nosync")) {
		pr_err("bgw_warnfix: warn=%px / patcher=%px are not __warn / aarch64_insn_patch_text_nosync\n",
		       (void *)warn, (void *)patcher);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(ctx); i++) {
		if (read_insn(warn + ctx[i].off, &insn) || insn != ctx[i].insn) {
			pr_err("bgw_warnfix: __warn+0x%x is %08x, expected %08x - unknown kernel, nothing changed\n",
			       ctx[i].off, insn, ctx[i].insn);
			return -ENODEV;
		}
	}
	if (read_insn(call, &orig_insn))
		return -EFAULT;
	if (orig_insn == new) {
		pr_err("bgw_warnfix: already patched\n");
		return -EEXIST;
	}
	if ((orig_insn & 0xfc000000) != 0x94000000) {
		pr_err("bgw_warnfix: __warn+0x%x is %08x, not a BL\n", CALL_OFF, orig_insn);
		return -ENODEV;
	}
	target = call + ((long)((s32)(orig_insn << 6) >> 6) << 2);
	if (!sym_is(target, "kpanic_write_file")) {
		pr_err("bgw_warnfix: the BL at __warn+0x%x does not call kpanic_write_file (%pS)\n", CALL_OFF, (void *)target);
		return -ENODEV;
	}

	ret = ((patch_fn)patcher)((void *)call, new);
	if (ret || read_insn(call, &insn) || insn != new) {
		pr_err("bgw_warnfix: patching failed (ret %d, now %08x)\n", ret, insn);
		return ret ? ret : -EIO;
	}
	patched = true;
	pr_info("bgw_warnfix: __warn+0x%x %08x (bl %pS) -> %08x (b __warn+0x%x): warnings no longer write a crash file\n",
		CALL_OFF, orig_insn, (void *)target, new, RESUME_OFF);
	return 0;
}

static void __exit warnfix_exit(void)
{
	u32 insn = 0;

	if (!patched)
		return;
	((patch_fn)patcher)((void *)(warn + CALL_OFF), orig_insn);
	read_insn(warn + CALL_OFF, &insn);
	pr_info("bgw_warnfix: restored __warn+0x%x = %08x%s\n", CALL_OFF, insn, insn == orig_insn ? "" : " (MISMATCH)");
}

module_init(warnfix_init);
module_exit(warnfix_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BGW320: WARN() no longer writes a crash file from the exception handler (which reset the box)");
