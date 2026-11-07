// SPDX-License-Identifier: GPL-2.0
/*
 * slicx - expose the ProSLIC channel objects of Broadcom's slicslac.ko (BGW320, Si32291) to user space.
 *
 * slicslac only exposes line mode / ring / hook through its ioctls, while the chip itself does tones, DTMF
 * detection, FSK caller ID and line tests. slicx gives register and RAM access to an *initialised* channel
 * through the chip's own control interface (the same function pointers the ProSLIC API uses, which take
 * slicslac's SPI mutex), so user space can drive those blocks.
 *
 * slicslac internals used (from slicslac.ko of the stock 5.15.167 image, verified by disassembly):
 *   endpoints[] at .bss+0x8c8, 40-byte entries: u32 device, u32 channel, u32 configured(@8)
 *   devices[]   at .bss+0x11d8, 0x1440 bytes per device, 0x98 per channel, ProSLIC chan ptr at +40
 *   chan obj:   +0 -> deviceType*, +8 u8 channel number, +12 u32 channelType (1 = PROSLIC)
 *   deviceType: +0 -> controlInterface*;  ctrlIf: +0 hCtrl, +16 WriteReg, +24 ReadReg, +32 WriteRAM, +40 ReadRAM
 * The data symbols are not in kallsyms, so the loader passes slicslac's .bss address
 * (/sys/module/slicslac/sections/.bss) as module parameter `slic_bss`.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include "slicx.h"

static unsigned long slic_bss;
module_param(slic_bss, ulong, 0444);
MODULE_PARM_DESC(slic_bss, "address of slicslac .bss (from /sys/module/slicslac/sections/.bss)");

#define EPT_OFF      0x8c8
#define EPT_SIZE     40
#define EPT_MAX      18
#define DEV_OFF      0x11d8
#define DEV_STRIDE   0x1440
#define CH_STRIDE    0x98
#define CH_PTR_OFF   40

typedef int  (*wreg_t)(void *h, u8 ch, u8 addr, u8 data);
typedef u8   (*rreg_t)(void *h, u8 ch, u8 addr);
typedef int  (*wram_t)(void *h, u8 ch, u16 addr, unsigned long data);
typedef unsigned long (*rram_t)(void *h, u8 ch, u16 addr);

struct ctrl_if { void *h; void *reset; wreg_t wreg; rreg_t rreg; wram_t wram; rram_t rram; };

static DEFINE_MUTEX(slx_lock);

static void *chan_obj(u32 line, int *err)
{
	u8 *ept, *chan;
	u32 dev, ch;
	if (!slic_bss) { *err = -ENODEV; return NULL; }
	if (line >= EPT_MAX) { *err = -EINVAL; return NULL; }
	ept = (u8 *)slic_bss + EPT_OFF + line * EPT_SIZE;
	if (*(u32 *)(ept + 8) != 1) { *err = -ENODATA; return NULL; }        /* line not configured (SLIC not initialised) */
	dev = *(u32 *)ept; ch = *(u32 *)(ept + 4);
	if (dev > 7 || ch > 15) { *err = -EIO; return NULL; }
	chan = *(u8 **)((u8 *)slic_bss + DEV_OFF + dev * DEV_STRIDE + ch * CH_STRIDE + CH_PTR_OFF);
	if ((unsigned long)chan < 0xffffff8000000000UL) { *err = -EIO; return NULL; }   /* must be a kernel (linear/vmalloc) address */
	if (*(u32 *)(chan + 12) != 1) { *err = -EPROTO; return NULL; }      /* not a ProSLIC channel */
	return chan;
}

static struct ctrl_if *ctrl_of(void *chan) { return **(struct ctrl_if ***)chan; }

static long slx_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct slx_rw rw;
	struct ctrl_if *c;
	void *chan;
	int err = 0;
	u8 chn;

	if (_IOC_TYPE(cmd) != SLX_MAGIC) return -ENOTTY;
	if (copy_from_user(&rw, (void __user *)arg, sizeof(rw))) return -EFAULT;
	mutex_lock(&slx_lock);
	chan = chan_obj(rw.line, &err);
	if (!chan) goto out;
	c = ctrl_of(chan);
	chn = *((u8 *)chan + 8);
	switch (cmd) {
	case SLX_RDREG: rw.val = c->rreg(c->h, chn, (u8)rw.addr); break;
	case SLX_WRREG: err = c->wreg(c->h, chn, (u8)rw.addr, (u8)rw.val); break;
	case SLX_RDRAM: rw.val = c->rram(c->h, chn, (u16)rw.addr); break;
	case SLX_WRRAM: err = c->wram(c->h, chn, (u16)rw.addr, (unsigned long)rw.val); break;
	case SLX_INFO:  rw.addr = chn; rw.val = *(u32 *)((u8 *)chan + 12); break;
	default: err = -ENOTTY;
	}
	if (err > 0) err = -EIO;
out:
	mutex_unlock(&slx_lock);
	if (!err && copy_to_user((void __user *)arg, &rw, sizeof(rw))) err = -EFAULT;
	return err;
}

static const struct file_operations slx_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = slx_ioctl,
	.compat_ioctl = slx_ioctl,
};
static struct miscdevice slx_dev = { .minor = MISC_DYNAMIC_MINOR, .name = "slicx", .fops = &slx_fops, .mode = 0600 };

static int __init slx_init(void)
{
	if (!slic_bss) { pr_err("slicx: slic_bss parameter missing\n"); return -EINVAL; }
	pr_info("slicx: slicslac .bss at %px\n", (void *)slic_bss);
	return misc_register(&slx_dev);
}
static void __exit slx_exit(void) { misc_deregister(&slx_dev); }
module_init(slx_init);
module_exit(slx_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ProSLIC register/RAM access for Broadcom slicslac (BGW320 FXS)");
