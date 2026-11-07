// Read-only probe of the eth1 multi-gig PHY (BCM54991L at MDIO 0x1e) clause-45 ability registers, to see whether the
// silicon can do 10GBASE-T (the device tree caps the port at 5G with caps-no-10000). Reads only; prints to dmesg.
#include <linux/module.h>
#include <linux/kernel.h>

extern int mdio_read_c45_register(uint32_t addr, uint32_t dev, uint16_t reg, uint16_t *val);

static uint addr = 0x1e;
module_param(addr, uint, 0444);

static const struct { u8 dev; u16 reg; const char *what; } regs[] = {
	{ 1, 0x0002, "PMA dev id hi" },          { 1, 0x0003, "PMA dev id lo" },
	{ 1, 0x0004, "PMA speed ability (b0=10G b13=2.5G b14=5G)" },
	{ 1, 0x000b, "PMA ext ability (b2=10GBASE-T, b14=2.5G/5G)" },
	{ 1, 0x0015, "PMA 2.5G/5G ext ability (b0=2.5GT b1=5GT)" },
	{ 1, 0x0007, "PMA control 2 (type select)" },
	{ 7, 0x0000, "AN control" },             { 7, 0x0001, "AN status" },
	{ 7, 0x0020, "MGBASE-T AN ctrl/adv (b12=10G b8=5G b7=2.5G)" },
	{ 7, 0x0021, "MGBASE-T AN status (LP b11=10G b6=5G b5=2.5G)" },
};

static int __init phyprobe_init(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u16 v = 0xdead;
		int rc = mdio_read_c45_register(addr, regs[i].dev, regs[i].reg, &v);
		pr_info("phyprobe: addr 0x%02x %u.0x%04x = 0x%04x rc=%d  %s\n", addr, regs[i].dev, regs[i].reg, v, rc, regs[i].what);
	}
	return 0;
}
static void __exit phyprobe_exit(void) { }
module_init(phyprobe_init);
module_exit(phyprobe_exit);
MODULE_LICENSE("GPL");
