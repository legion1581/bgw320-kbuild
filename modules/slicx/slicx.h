/* shared between slicx.ko and user space */
#ifndef SLICX_H
#define SLICX_H
#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif
struct slx_rw { __u32 line; __u32 addr; __u64 val; };
#define SLX_MAGIC 'x'
#define SLX_RDREG _IOWR(SLX_MAGIC, 1, struct slx_rw)
#define SLX_WRREG _IOW (SLX_MAGIC, 2, struct slx_rw)
#define SLX_RDRAM _IOWR(SLX_MAGIC, 3, struct slx_rw)
#define SLX_WRRAM _IOW (SLX_MAGIC, 4, struct slx_rw)
#define SLX_INFO  _IOWR(SLX_MAGIC, 5, struct slx_rw)
#endif
