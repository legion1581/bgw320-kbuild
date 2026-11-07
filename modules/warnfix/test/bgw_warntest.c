// bgw_warntest: TEST ONLY - triggers one WARN() in process context (insmod) and refuses to stay loaded.
// Without bgw_warnfix loaded this resets the BGW320 (see bgw_warnfix.c). Never ship in the image.
#include <linux/module.h>

static int __init warntest_init(void)
{
	pr_info("bgw_warntest: triggering a deliberate WARN()\n");
	WARN(1, "bgw_warntest: deliberate warning from process context\n");
	pr_info("bgw_warntest: returned from WARN(), the system is still running\n");
	return -EAGAIN;
}

module_init(warntest_init);
MODULE_LICENSE("GPL");
